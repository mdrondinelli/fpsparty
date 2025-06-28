#include "replicated_game.hpp"
#include "game_core/humanoid_input_state.hpp"
#include "game_core/humanoid_movement.hpp"
#include "game_core/projectile_movement.hpp"
#include "serial/serialize.hpp"
#include <algorithm>
#include <vector>

namespace fpsparty::game {
struct Replicated_game::Impl {
  std::vector<std::unique_ptr<Replicated_humanoid::Impl>> humanoid_impls{};
  std::vector<std::unique_ptr<Replicated_projectile::Impl>> projectile_impls{};
};

struct Replicated_humanoid::Impl {
  std::uint32_t network_id{};
  game_core::Humanoid_input_state input_state{};
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

std::uint32_t Replicated_humanoid::get_network_id() const noexcept {
  return _impl->network_id;
}

const game_core::Humanoid_input_state &
Replicated_humanoid::get_input_state() const noexcept {
  return _impl->input_state;
}

std::optional<std::uint16_t>
Replicated_humanoid::get_input_sequence_number() const noexcept {
  return _impl->input_sequence_number;
}

void Replicated_humanoid::set_input_state(
    const game_core::Humanoid_input_state &input_state) const noexcept {
  _impl->input_state = input_state;
}

void Replicated_humanoid::set_input_state(
    const game_core::Humanoid_input_state &input_state,
    std::uint16_t input_sequence_number) const noexcept {
  _impl->input_state = input_state;
  _impl->input_sequence_number = input_sequence_number;
}

const Eigen::Vector3f &Replicated_humanoid::get_position() const noexcept {
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

void Replicated_game::clear() const { _impl->humanoid_impls.clear(); }

void Replicated_game::simulate(const Simulate_info &info) const {
  for (const auto &humanoid_impl : _impl->humanoid_impls) {
    const auto movement_result = game_core::simulate_humanoid_movement({
        .initial_position = humanoid_impl->position,
        .input_state = humanoid_impl->input_state,
        .duration = info.duration,
    });
    humanoid_impl->position = movement_result.final_position;
  }
  for (const auto &projectile_impl : _impl->projectile_impls) {
    const auto movement_result = game_core::simulate_projectile_movement({
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
  const auto humanoid_count = deserialize<std::uint8_t>(reader);
  if (!humanoid_count) {
    throw Snapshot_application_error{};
  }
  for (const auto &humanoid_impl : _impl->humanoid_impls) {
    humanoid_impl->marked = false;
  }
  for (auto i = 0u; i != *humanoid_count; ++i) {
    const auto humanoid_network_id = deserialize<std::uint32_t>(reader);
    if (!humanoid_network_id) {
      throw Snapshot_application_error{};
    }
    const auto humanoid_impl = [&]() {
      auto retval = get_humanoid_by_network_id(*humanoid_network_id)._impl;
      if (!retval) {
        retval =
            _impl->humanoid_impls
                .emplace_back(std::make_unique<Replicated_humanoid::Impl>())
                .get();
        retval->network_id = *humanoid_network_id;
      }
      return retval;
    }();
    humanoid_impl->marked = true;
    const auto humanoid_position_x = deserialize<float>(reader);
    if (!humanoid_position_x) {
      throw Snapshot_application_error{};
    }
    const auto humanoid_position_y = deserialize<float>(reader);
    if (!humanoid_position_y) {
      throw Snapshot_application_error{};
    }
    const auto humanoid_position_z = deserialize<float>(reader);
    if (!humanoid_position_z) {
      throw Snapshot_application_error{};
    }
    const auto input_state =
        deserialize<game_core::Humanoid_input_state>(reader);
    if (!input_state) {
      throw Snapshot_application_error{};
    }
    const auto input_sequence_number =
        deserialize<std::optional<std::uint16_t>>(reader);
    if (!input_sequence_number) {
      throw Snapshot_application_error{};
    }
    humanoid_impl->position.x() = *humanoid_position_x;
    humanoid_impl->position.y() = *humanoid_position_y;
    humanoid_impl->position.z() = *humanoid_position_z;
    humanoid_impl->input_state = *input_state;
    humanoid_impl->input_sequence_number = *input_sequence_number;
  }
  for (auto it = _impl->humanoid_impls.begin();
       it != _impl->humanoid_impls.end();) {
    if (!(*it)->marked) {
      it = _impl->humanoid_impls.erase(it);
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

std::pmr::vector<Replicated_humanoid> Replicated_game::get_humanoids(
    std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Replicated_humanoid>(memory_resource);
  retval.reserve(_impl->humanoid_impls.size());
  for (const auto &humanoid_impl : _impl->humanoid_impls) {
    retval.emplace_back(Replicated_humanoid{humanoid_impl.get()});
  }
  return retval;
}

Replicated_humanoid Replicated_game::get_humanoid_by_network_id(
    std::uint32_t network_id) const noexcept {
  const auto it = std::ranges::find_if(
      _impl->humanoid_impls, [&](const auto &humanoid_impl) {
        return humanoid_impl->network_id == network_id;
      });
  return Replicated_humanoid{it != _impl->humanoid_impls.end() ? it->get()
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
