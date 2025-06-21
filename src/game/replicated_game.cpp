#include "replicated_game.hpp"
#include "serial/serialize.hpp"
#include <algorithm>
#include <vector>

namespace fpsparty::game {
struct Replicated_player::Impl {
  bool marked{};
  Eigen::Vector3f position{0.0f, 0.0f, 0.0f};
  float yaw{};
  float pitch{};
};

const Eigen::Vector3f &Replicated_player::get_position() const noexcept {
  return _impl->position;
}

float Replicated_player::get_yaw() const noexcept { return _impl->yaw; }

void Replicated_player::set_yaw(float yaw) const noexcept { _impl->yaw = yaw; }

float Replicated_player::get_pitch() const noexcept { return _impl->pitch; }

void Replicated_player::set_pitch(float pitch) const noexcept {
  _impl->pitch = pitch;
}

struct Replicated_game::Impl {
  std::vector<
      std::pair<std::uint32_t, std::unique_ptr<Replicated_player::Impl>>>
      players{};
  std::vector<std::uint32_t> local_player_ids{};
};

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
    const auto yaw = deserialize<float>(reader);
    if (!yaw) {
      throw Snapshot_application_error{};
    }
    const auto pitch = deserialize<float>(reader);
    if (!pitch) {
      throw Snapshot_application_error{};
    }
    player_impl->position.x() = *player_position_x;
    player_impl->position.y() = *player_position_y;
    player_impl->position.z() = *player_position_z;
    if (!is_player_locally_controlled(*player_id)) {
      player_impl->yaw = *yaw;
      player_impl->pitch = *pitch;
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
