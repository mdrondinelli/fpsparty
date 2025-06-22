#include "replicated_game.hpp"
#include "game/game.hpp"
#include "game/humanoid_movement.hpp"
#include "serial/serialize.hpp"
#include <algorithm>
#include <vector>

namespace fpsparty::game {
struct Replicated_player::Impl {
  bool marked{};
  Player::Input_state input_state{};
  std::optional<std::uint16_t> input_sequence_number{};
  Eigen::Vector3f position{0.0f, 0.0f, 0.0f};
};

const Player::Input_state &Replicated_player::get_input_state() const noexcept {
  return _impl->input_state;
}

std::optional<std::uint16_t>
Replicated_player::get_input_sequence_number() const noexcept {
  return _impl->input_sequence_number;
}

void Replicated_player::set_input_state(
    const Player::Input_state &input_state,
    std::uint16_t input_sequence_number) const noexcept {
  _impl->input_state = input_state;
  _impl->input_sequence_number = input_sequence_number;
}

const Eigen::Vector3f &Replicated_player::get_position() const noexcept {
  return _impl->position;
}

struct Replicated_game::Impl {
  std::vector<
      std::pair<std::uint32_t, std::unique_ptr<Replicated_player::Impl>>>
      players{};
  std::vector<std::uint32_t> local_player_ids{};
};

void Replicated_game::clear() const {
  _impl->players.clear();
  _impl->local_player_ids.clear();
}

void Replicated_game::simulate(const Simulate_info &info) const {
  for (const auto &player_node : _impl->players) {
    const auto movement_result = simulate_humanoid_movement({
        .initial_position = player_node.second->position,
        .input_state = player_node.second->input_state,
        .duration = info.duration,
    });
    player_node.second->position = movement_result.final_position;
  }
}

void Replicated_game::apply_snapshot(serial::Reader &reader) const {
  using serial::deserialize;
  const auto player_count = deserialize<std::uint8_t>(reader);
  if (!player_count) {
    throw Snapshot_application_error{};
  }
  for (auto it = _impl->players.begin(); it != _impl->players.end(); ++it) {
    it->second->marked = false;
  }
  for (auto i = 0u; i != *player_count; ++i) {
    const auto player_id = deserialize<std::uint32_t>(reader);
    if (!player_id) {
      throw Snapshot_application_error{};
    }
    const auto player_impl = [&]() {
      auto retval = get_player(*player_id)._impl;
      if (!retval) {
        retval = _impl->players
                     .emplace_back(*player_id,
                                   std::make_unique<Replicated_player::Impl>())
                     .second.get();
      }
      return retval;
    }();
    player_impl->marked = true;
    const auto player_position_x = deserialize<float>(reader);
    if (!player_position_x) {
      throw Snapshot_application_error{};
    }
    const auto player_position_y = deserialize<float>(reader);
    if (!player_position_y) {
      throw Snapshot_application_error{};
    }
    const auto player_position_z = deserialize<float>(reader);
    if (!player_position_z) {
      throw Snapshot_application_error{};
    }
    const auto input_state = deserialize<Player::Input_state>(reader);
    if (!input_state) {
      throw Snapshot_application_error{};
    }
    const auto input_sequence_number =
        deserialize<std::optional<std::uint16_t>>(reader);
    if (!input_sequence_number) {
      throw Snapshot_application_error{};
    }
    player_impl->position.x() = *player_position_x;
    player_impl->position.y() = *player_position_y;
    player_impl->position.z() = *player_position_z;
    if (!is_player_locally_controlled(*player_id)) {
      player_impl->input_state = *input_state;
      player_impl->input_sequence_number = *input_sequence_number;
    }
  }
  for (auto it = _impl->players.begin(); it != _impl->players.end();) {
    if (!it->second->marked) {
      it = _impl->players.erase(it);
    } else {
      ++it;
    }
  }
}

Replicated_player Replicated_game::get_player(std::uint32_t id) const noexcept {
  const auto it = std::ranges::find_if(
      _impl->players, [&](const auto &node) { return node.first == id; });
  return Replicated_player{it != _impl->players.end() ? it->second.get()
                                                      : nullptr};
}

std::pmr::vector<Replicated_player>
Replicated_game::get_players(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Replicated_player>(memory_resource);
  retval.reserve(_impl->players.size());
  for (const auto &node : _impl->players) {
    retval.emplace_back(Replicated_player{node.second.get()});
  }
  return retval;
}

bool Replicated_game::is_player_locally_controlled(
    std::uint32_t id) const noexcept {
  return std::ranges::find(_impl->local_player_ids, id) !=
         _impl->local_player_ids.end();
}

void Replicated_game::set_player_locally_controlled(std::uint32_t id,
                                                    bool b) const noexcept {
  if (b) {
    if (!is_player_locally_controlled(id)) {
      _impl->local_player_ids.emplace_back(id);
    }
  } else {
    const auto it = std::ranges::find(_impl->local_player_ids, id);
    if (it != _impl->local_player_ids.end()) {
      _impl->local_player_ids.erase(it);
    }
  }
}

Replicated_game create_replicated_game(const Replicated_game::Create_info &) {
  return Replicated_game{new Replicated_game::Impl};
}

void destroy_replicated_game(Replicated_game replicated_game) noexcept {
  delete replicated_game._impl;
}
}; // namespace fpsparty::game
