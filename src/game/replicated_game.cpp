#include "replicated_game.hpp"
#include "game/game.hpp"
#include "game/humanoid_movement.hpp"
#include "game/projectile_movement.hpp"
#include "serial/serialize.hpp"
#include <algorithm>
#include <vector>

namespace fpsparty::game {
struct Replicated_game::Impl {
  std::vector<std::unique_ptr<Replicated_player::Impl>> player_impls{};
  std::vector<std::unique_ptr<Replicated_projectile::Impl>> projectile_impls{};
};

struct Replicated_player::Impl {
  std::uint32_t network_id{};
  Player::Input_state input_state{};
  std::optional<std::uint16_t> input_sequence_number{};
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  bool marked{};
};

struct Replicated_projectile::Impl {
  std::uint32_t network_id{};
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
  bool marked{};
};

std::uint32_t Replicated_player::get_network_id() const noexcept {
  return _impl->network_id;
}

const Player::Input_state &Replicated_player::get_input_state() const noexcept {
  return _impl->input_state;
}

std::optional<std::uint16_t>
Replicated_player::get_input_sequence_number() const noexcept {
  return _impl->input_sequence_number;
}

void Replicated_player::set_input_state(
    const Player::Input_state &input_state) const noexcept {
  _impl->input_state = input_state;
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

std::uint32_t Replicated_projectile::get_network_id() const noexcept {
  return _impl->network_id;
}

const Eigen::Vector3f &Replicated_projectile::get_position() const noexcept {
  return _impl->position;
}

const Eigen::Vector3f &Replicated_projectile::get_velocity() const noexcept {
  return _impl->velocity;
}

void Replicated_game::clear() const { _impl->player_impls.clear(); }

void Replicated_game::simulate(const Simulate_info &info) const {
  for (const auto &player_impl : _impl->player_impls) {
    const auto movement_result = simulate_humanoid_movement({
        .initial_position = player_impl->position,
        .input_state = player_impl->input_state,
        .duration = info.duration,
    });
    player_impl->position = movement_result.final_position;
  }
  for (const auto &projectile_impl : _impl->projectile_impls) {
    const auto movement_result = simulate_projectile_movement({
        .initial_position = projectile_impl->position,
        .initial_velocity = projectile_impl->velocity,
        .duration = info.duration,
    });
    projectile_impl->position = movement_result.final_position;
    projectile_impl->velocity = movement_result.final_velocity;
  }
}

void Replicated_game::apply_snapshot(serial::Reader &reader) const {
  using serial::deserialize;
  const auto player_count = deserialize<std::uint8_t>(reader);
  if (!player_count) {
    throw Snapshot_application_error{};
  }
  for (const auto &player_impl : _impl->player_impls) {
    player_impl->marked = false;
  }
  for (auto i = 0u; i != *player_count; ++i) {
    const auto player_network_id = deserialize<std::uint32_t>(reader);
    if (!player_network_id) {
      throw Snapshot_application_error{};
    }
    const auto player_impl = [&]() {
      auto retval = get_player_by_network_id(*player_network_id)._impl;
      if (!retval) {
        retval = _impl->player_impls
                     .emplace_back(std::make_unique<Replicated_player::Impl>())
                     .get();
        retval->network_id = *player_network_id;
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
    player_impl->input_state = *input_state;
    player_impl->input_sequence_number = *input_sequence_number;
  }
  for (auto it = _impl->player_impls.begin();
       it != _impl->player_impls.end();) {
    if (!(*it)->marked) {
      it = _impl->player_impls.erase(it);
    } else {
      ++it;
    }
  }
  const auto projectile_count = deserialize<std::uint16_t>(reader);
  if (!projectile_count) {
    throw Snapshot_application_error{};
  }
  for (const auto &projectile_impl : _impl->projectile_impls) {
    projectile_impl->marked = false;
  }
  for (auto i = 0u; i != *projectile_count; ++i) {
    const auto projectile_network_id = deserialize<std::uint32_t>(reader);
    if (!projectile_network_id) {
      throw Snapshot_application_error{};
    }
    const auto projectile_impl = [&]() {
      auto retval = get_projectile_by_network_id(*projectile_network_id)._impl;
      if (!retval) {
        retval =
            _impl->projectile_impls
                .emplace_back(std::make_unique<Replicated_projectile::Impl>())
                .get();
        retval->network_id = *projectile_network_id;
      }
      return retval;
    }();
    projectile_impl->marked = true;
    const auto projectile_position_x = deserialize<float>(reader);
    if (!projectile_position_x) {
      throw Snapshot_application_error{};
    }
    const auto projectile_position_y = deserialize<float>(reader);
    if (!projectile_position_y) {
      throw Snapshot_application_error{};
    }
    const auto projectile_position_z = deserialize<float>(reader);
    if (!projectile_position_z) {
      throw Snapshot_application_error{};
    }
    projectile_impl->position.x() = *projectile_position_x;
    projectile_impl->position.y() = *projectile_position_y;
    projectile_impl->position.z() = *projectile_position_z;
    const auto projectile_velocity_x = deserialize<float>(reader);
    if (!projectile_velocity_x) {
      throw Snapshot_application_error{};
    }
    const auto projectile_velocity_y = deserialize<float>(reader);
    if (!projectile_velocity_y) {
      throw Snapshot_application_error{};
    }
    const auto projectile_velocity_z = deserialize<float>(reader);
    if (!projectile_velocity_z) {
      throw Snapshot_application_error{};
    }
    projectile_impl->velocity.x() = *projectile_velocity_x;
    projectile_impl->velocity.y() = *projectile_velocity_y;
    projectile_impl->velocity.z() = *projectile_velocity_z;
  }
  for (auto it = _impl->projectile_impls.begin();
       it != _impl->projectile_impls.end();) {
    if (!(*it)->marked) {
      it = _impl->projectile_impls.erase(it);
    } else {
      ++it;
    }
  }
}

std::pmr::vector<Replicated_player>
Replicated_game::get_players(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Replicated_player>(memory_resource);
  retval.reserve(_impl->player_impls.size());
  for (const auto &player_impl : _impl->player_impls) {
    retval.emplace_back(Replicated_player{player_impl.get()});
  }
  return retval;
}

Replicated_player Replicated_game::get_player_by_network_id(
    std::uint32_t network_id) const noexcept {
  const auto it =
      std::ranges::find_if(_impl->player_impls, [&](const auto &player_impl) {
        return player_impl->network_id == network_id;
      });
  return Replicated_player{it != _impl->player_impls.end() ? it->get()
                                                           : nullptr};
}

std::pmr::vector<Replicated_projectile> Replicated_game::get_projectiles(
    std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Replicated_projectile>(memory_resource);
  retval.reserve(_impl->projectile_impls.size());
  for (const auto &projectile_impl : _impl->projectile_impls) {
    retval.emplace_back(Replicated_projectile{projectile_impl.get()});
  }
  return retval;
}

Replicated_projectile Replicated_game::get_projectile_by_network_id(
    std::uint32_t network_id) const noexcept {
  const auto it = std::ranges::find_if(
      _impl->projectile_impls, [&](const auto &projectile_impl) {
        return projectile_impl->network_id == network_id;
      });
  return Replicated_projectile{it != _impl->projectile_impls.end() ? it->get()
                                                                   : nullptr};
}

Replicated_game create_replicated_game(const Replicated_game::Create_info &) {
  return Replicated_game{new Replicated_game::Impl};
}

void destroy_replicated_game(Replicated_game replicated_game) noexcept {
  delete replicated_game._impl;
}
}; // namespace fpsparty::game
