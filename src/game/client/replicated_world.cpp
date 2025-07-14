#include "replicated_world.hpp"
#include "game/client/replicated_player.hpp"
#include "game/client/replicated_projectile.hpp"
#include "game/core/entity.hpp"
#include "game/core/entity_id.hpp"
#include "game/core/humanoid_input_state.hpp"
#include "game/core/sequence_number.hpp"
#include <algorithm>

namespace fpsparty::game {
void Replicated_world::load(const Replicated_world_load_info &info) {
  using serial::deserialize;
  const auto humanoid_count =
      deserialize<std::uint8_t>(*info.public_state_reader);
  if (!humanoid_count) {
    throw Replicated_world_load_error{};
  }
  auto temp_humanoids = std::vector<rc::Strong<Replicated_humanoid>>{};
  temp_humanoids.reserve(*humanoid_count);
  for (auto i = std::uint8_t{}; i != humanoid_count; ++i) {
    const auto entity_id =
        deserialize<Entity_id>(*info.public_state_reader);
    if (!entity_id) {
      throw Replicated_world_load_error{};
    }
    const auto position_x = deserialize<float>(*info.public_state_reader);
    if (!position_x) {
      throw Replicated_world_load_error{};
    }
    const auto position_y = deserialize<float>(*info.public_state_reader);
    if (!position_y) {
      throw Replicated_world_load_error{};
    }
    const auto position_z = deserialize<float>(*info.public_state_reader);
    if (!position_z) {
      throw Replicated_world_load_error{};
    }
    const auto input_state =
        deserialize<Humanoid_input_state>(*info.public_state_reader);
    if (!input_state) {
      throw Replicated_world_load_error{};
    }
    auto humanoid = [&]() {
      const auto existing_humanoid =
          get_humanoid_by_entity_id(*entity_id);
      return existing_humanoid ? existing_humanoid
                               : _humanoid_factory.create(*entity_id);
    }();
    humanoid->set_position({*position_x, *position_y, *position_z});
    humanoid->set_input_state(*input_state);
    temp_humanoids.emplace_back(std::move(humanoid));
  }
  std::swap(_humanoids, temp_humanoids);
  for (const auto &humanoid : temp_humanoids) {
    if (!get_humanoid_by_entity_id(humanoid->get_entity_id())) {
      detail::on_remove_entity(*humanoid);
    }
  }
  const auto projectile_count =
      deserialize<std::uint16_t>(*info.public_state_reader);
  if (!projectile_count) {
    throw Replicated_world_load_error{};
  }
  auto temp_projectiles = std::vector<rc::Strong<Replicated_projectile>>{};
  temp_projectiles.reserve(*projectile_count);
  for (auto i = std::uint16_t{}; i != projectile_count; ++i) {
    const auto entity_id =
        deserialize<Entity_id>(*info.public_state_reader);
    const auto position_x = deserialize<float>(*info.public_state_reader);
    const auto position_y = deserialize<float>(*info.public_state_reader);
    const auto position_z = deserialize<float>(*info.public_state_reader);
    const auto velocity_x = deserialize<float>(*info.public_state_reader);
    const auto velocity_y = deserialize<float>(*info.public_state_reader);
    const auto velocity_z = deserialize<float>(*info.public_state_reader);
    auto projectile = [&]() {
      const auto existing_projectile =
          get_projectile_by_entity_id(*entity_id);
      return existing_projectile ? existing_projectile
                                 : _projectile_factory.create(*entity_id);
    }();
    projectile->set_position({*position_x, *position_y, *position_z});
    projectile->set_velocity({*velocity_x, *velocity_y, *velocity_z});
    temp_projectiles.emplace_back(std::move(projectile));
  }
  std::swap(_projectiles, temp_projectiles);
  for (const auto &projectile : temp_projectiles) {
    if (!get_projectile_by_entity_id(projectile->get_entity_id())) {
      detail::on_remove_entity(*projectile);
    }
  }
  auto temp_players = std::vector<rc::Strong<Replicated_player>>{};
  temp_players.reserve(info.player_state_count);
  for (auto i = std::uint8_t{}; i != info.player_state_count; ++i) {
    const auto entity_id =
        deserialize<Entity_id>(*info.player_state_reader);
    if (!entity_id) {
      throw Replicated_world_load_error{};
    }
    auto player = [&]() {
      const auto existing_player =
          get_player_by_entity_id(*entity_id);
      return existing_player ? existing_player
                             : _player_factory.create(*entity_id);
    }();
    const auto humanoid_entity_id =
        deserialize<Entity_id>(*info.player_state_reader);
    if (!humanoid_entity_id) {
      throw Replicated_world_load_error{};
    }
    if (*humanoid_entity_id) {
      const auto humanoid =
          get_humanoid_by_entity_id(*humanoid_entity_id);
      if (!humanoid) {
        throw Replicated_world_load_error{};
      }
      const auto input_state =
          deserialize<Humanoid_input_state>(*info.player_state_reader);
      if (!input_state) {
        throw Replicated_world_load_error{};
      }
      const auto input_sequence_number =
          deserialize<std::optional<Sequence_number>>(
              *info.player_state_reader);
      if (!input_sequence_number) {
        throw Replicated_world_load_error{};
      }
      player->set_humanoid(humanoid);
      player->set_input_state(*input_state);
      player->set_input_sequence_number(*input_sequence_number);
    }
    temp_players.emplace_back(std::move(player));
  }
  std::swap(_players, temp_players);
  for (const auto &player : temp_players) {
    if (!get_player_by_entity_id(player->get_entity_id())) {
      detail::on_remove_entity(*player);
    }
  }
}

void Replicated_world::reset() {
  while (!_players.empty()) {
    const auto player = std::move(_players.back());
    detail::on_remove_entity(*player);
    _players.pop_back();
  }
  while (!_humanoids.empty()) {
    const auto humanoid = std::move(_humanoids.back());
    detail::on_remove_entity(*humanoid);
    _humanoids.pop_back();
  }
  while (!_projectiles.empty()) {
    const auto projectile = std::move(_humanoids.back());
    detail::on_remove_entity(*projectile);
    _projectiles.pop_back();
  }
}

rc::Strong<Replicated_player> Replicated_world::get_player_by_entity_id(
    Entity_id id) const noexcept {
  const auto it = std::ranges::find_if(
      _players, [id](const rc::Strong<Replicated_player> &x) {
        return x->get_entity_id() == id;
      });
  return it != _players.end() ? *it : nullptr;
}

rc::Strong<Replicated_humanoid>
Replicated_world::get_humanoid_by_entity_id(
    Entity_id id) const noexcept {
  const auto it = std::ranges::find_if(
      _humanoids, [id](const rc::Strong<Replicated_humanoid> &x) {
        return x->get_entity_id() == id;
      });
  return it != _humanoids.end() ? *it : nullptr;
}

rc::Strong<Replicated_projectile>
Replicated_world::get_projectile_by_entity_id(
    Entity_id id) const noexcept {
  const auto it = std::ranges::find_if(
      _projectiles, [id](const rc::Strong<Replicated_projectile> &x) {
        return x->get_entity_id() == id;
      });
  return it != _projectiles.end() ? *it : nullptr;
}

std::pmr::vector<rc::Strong<Replicated_player>> Replicated_world::get_players(
    std::pmr::memory_resource *memory_resource) const {
  auto retval =
      std::pmr::vector<rc::Strong<Replicated_player>>{memory_resource};
  retval.assign_range(_players);
  return retval;
}

std::pmr::vector<rc::Strong<Replicated_humanoid>>
Replicated_world::get_humanoids(
    std::pmr::memory_resource *memory_resource) const {
  auto retval =
      std::pmr::vector<rc::Strong<Replicated_humanoid>>{memory_resource};
  retval.assign_range(_humanoids);
  return retval;
}

std::pmr::vector<rc::Strong<Replicated_projectile>>
Replicated_world::get_projectiles(
    std::pmr::memory_resource *memory_resource) const {
  auto retval =
      std::pmr::vector<rc::Strong<Replicated_projectile>>{memory_resource};
  retval.assign_range(_projectiles);
  return retval;
}

std::size_t Replicated_world::get_player_count() const noexcept {
  return _players.size();
}

std::size_t Replicated_world::get_humanoid_count() const noexcept {
  return _humanoids.size();
}

std::size_t Replicated_world::get_projectile_count() const noexcept {
  return _projectiles.size();
}
} // namespace fpsparty::game
