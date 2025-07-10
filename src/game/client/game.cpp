#include "game.hpp"
#include "game_core/humanoid_input_state.hpp"
#include "game_core/humanoid_movement.hpp"
#include "game_core/projectile_movement.hpp"
#include "rc.hpp"
#include "serial/serialize.hpp"
#include <algorithm>
#include <memory_resource>
#include <vector>

namespace fpsparty::game_replica {
Game::Game(const Game_create_info &)
    : _player_memory{rc::make_unique_memory_resource_for_factory<Player>()},
      _humanoid_memory{rc::make_unique_memory_resource_for_factory<Humanoid>()},
      _projectile_memory{
          rc::make_unique_memory_resource_for_factory<Projectile>()},
      _player_factory{_player_memory.get()},
      _humanoid_factory{_humanoid_memory.get()},
      _projectile_factory{_projectile_memory.get()} {}

void Game::clear() {
  _world_humanoids.clear();
  _world_projectiles.clear();
}

void Game::simulate(const Game_simulate_info &info) {
  for (const auto &humanoid : _world_humanoids) {
    const auto movement_result = game_core::simulate_humanoid_movement({
        .initial_position = humanoid->get_position(),
        .input_state = humanoid->get_input_state(),
        .duration = info.duration,
    });
    humanoid->set_position(movement_result.final_position);
  }
  for (const auto &projectile : _world_projectiles) {
    const auto movement_result = game_core::simulate_projectile_movement({
        .initial_position = projectile->get_position(),
        .initial_velocity = projectile->get_velocity(),
        .duration = info.duration,
    });
    projectile->set_position(movement_result.final_position);
    projectile->set_velocity(movement_result.final_velocity);
  }
}

void Game::load_world_state(serial::Reader &reader) {
  using serial::deserialize;
  const auto humanoid_count = deserialize<std::uint8_t>(reader);
  if (!humanoid_count) {
    throw Game_snapshot_application_error{};
  }
  auto humanoids = std::vector<rc::Strong<Humanoid>>{};
  humanoids.reserve(*humanoid_count);
  for (auto i = 0u; i != *humanoid_count; ++i) {
    const auto humanoid_network_id = deserialize<std::uint32_t>(reader);
    if (!humanoid_network_id) {
      throw Game_snapshot_application_error{};
    }
    const auto humanoid = [&]() {
      auto retval = get_humanoid_by_network_id(*humanoid_network_id);
      if (!retval) {
        retval = _humanoid_factory.create(*humanoid_network_id);
      }
      return retval;
    }();
    humanoids.emplace_back(humanoid);
    const auto humanoid_position_x = deserialize<float>(reader);
    if (!humanoid_position_x) {
      throw Game_snapshot_application_error{};
    }
    const auto humanoid_position_y = deserialize<float>(reader);
    if (!humanoid_position_y) {
      throw Game_snapshot_application_error{};
    }
    const auto humanoid_position_z = deserialize<float>(reader);
    if (!humanoid_position_z) {
      throw Game_snapshot_application_error{};
    }
    const auto input_state =
        deserialize<game_core::Humanoid_input_state>(reader);
    if (!input_state) {
      throw Game_snapshot_application_error{};
    }
    const auto input_sequence_number =
        deserialize<std::optional<std::uint16_t>>(reader);
    if (!input_sequence_number) {
      throw Game_snapshot_application_error{};
    }
    humanoid->set_position({
        *humanoid_position_x,
        *humanoid_position_y,
        *humanoid_position_z,
    });
    humanoid->set_input_state(*input_state);
  }
  _world_humanoids = std::move(humanoids);
  const auto projectile_count = deserialize<std::uint16_t>(reader);
  if (!projectile_count) {
    throw Game_snapshot_application_error{};
  }
  auto projectiles = std::vector<rc::Strong<Projectile>>{};
  projectiles.reserve(*projectile_count);
  for (auto i = 0u; i != *projectile_count; ++i) {
    const auto projectile_network_id = deserialize<std::uint32_t>(reader);
    if (!projectile_network_id) {
      throw Game_snapshot_application_error{};
    }
    const auto projectile = [&]() {
      auto retval = get_projectile_by_network_id(*projectile_network_id);
      if (!retval) {
        retval = _projectile_factory.create(*projectile_network_id);
      }
      return retval;
    }();
    projectiles.emplace_back(projectile);
    const auto projectile_position_x = deserialize<float>(reader);
    if (!projectile_position_x) {
      throw Game_snapshot_application_error{};
    }
    const auto projectile_position_y = deserialize<float>(reader);
    if (!projectile_position_y) {
      throw Game_snapshot_application_error{};
    }
    const auto projectile_position_z = deserialize<float>(reader);
    if (!projectile_position_z) {
      throw Game_snapshot_application_error{};
    }
    projectile->set_position({
        *projectile_position_x,
        *projectile_position_y,
        *projectile_position_z,
    });
    const auto projectile_velocity_x = deserialize<float>(reader);
    if (!projectile_velocity_x) {
      throw Game_snapshot_application_error{};
    }
    const auto projectile_velocity_y = deserialize<float>(reader);
    if (!projectile_velocity_y) {
      throw Game_snapshot_application_error{};
    }
    const auto projectile_velocity_z = deserialize<float>(reader);
    if (!projectile_velocity_z) {
      throw Game_snapshot_application_error{};
    }
    projectile->set_velocity({
        *projectile_velocity_x,
        *projectile_velocity_y,
        *projectile_velocity_z,
    });
  }
  _world_projectiles = std::move(projectiles);
}

void Game::load_player_state(serial::Reader &reader) {
  using serial::deserialize;
  const auto player_network_id = deserialize<std::uint32_t>(reader);
  if (!player_network_id) {
    throw Game_snapshot_application_error{};
  }
  const auto player = [&]() {
    auto retval = get_player_by_network_id(*player_network_id);
    if (!retval) {
      retval = _player_factory.create(*player_network_id);
      _world_players.emplace_back(retval);
    }
    return retval;
  }();
  const auto humanoid_network_id = deserialize<std::uint32_t>(reader);
  if (!humanoid_network_id) {
    throw Game_snapshot_application_error{};
  }
  if (*humanoid_network_id) {
    const auto humanoid = get_humanoid_by_network_id(*humanoid_network_id);
    if (humanoid) {
      player->set_humanoid(humanoid);
    }
    const auto input_state =
        deserialize<game_core::Humanoid_input_state>(reader);
    if (!input_state) {
      throw Game_snapshot_application_error{};
    }
    const auto input_sequence_number =
        deserialize<std::optional<std::uint16_t>>(reader);
    if (!input_sequence_number) {
      throw Game_snapshot_application_error{};
    }
    player->set_input_state(*input_state);
    player->set_input_sequence_number(*input_sequence_number);
  }
}

rc::Strong<Player>
Game::get_player_by_network_id(std::uint32_t network_id) const noexcept {
  const auto it = std::ranges::find_if(
      _world_players, [&](const rc::Strong<Player> &player) {
        return player->get_network_id() == network_id;
      });
  return it != _world_players.end() ? *it : nullptr;
}

std::pmr::vector<Humanoid>
Game::get_humanoids(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Humanoid>(memory_resource);
  retval.reserve(_impl->humanoid_impls.size());
  for (const auto &humanoid_impl : _impl->humanoid_impls) {
    retval.emplace_back(Humanoid{humanoid_impl.get()});
  }
  return retval;
}

rc::Strong<Humanoid>
Game::get_humanoid_by_network_id(std::uint32_t network_id) const noexcept {
  const auto it = std::ranges::find_if(
      _world_humanoids, [&](const rc::Strong<Humanoid> &humanoid) {
        return humanoid->get_network_id() == network_id;
      });
  return it != _world_humanoids.end() ? *it : nullptr;
}

std::pmr::vector<Projectile>
Game::get_projectiles(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Projectile>(memory_resource);
  retval.reserve(_impl->projectile_impls.size());
  for (const auto &projectile_impl : _impl->projectile_impls) {
    retval.emplace_back(Projectile{projectile_impl.get()});
  }
  return retval;
}

rc::Strong<Projectile>
Game::get_projectile_by_network_id(std::uint32_t network_id) const noexcept {
  const auto it = std::ranges::find_if(
      _world_projectiles, [&](const rc::Strong<Projectile> &projectile) {
        return projectile->get_network_id() == network_id;
      });
  return it != _world_projectiles.end() ? *it : nullptr;
}
}; // namespace fpsparty::game_replica
