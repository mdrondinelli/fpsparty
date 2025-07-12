#include "world.hpp"
#include "algorithms/unordered_erase.hpp"

namespace fpsparty::game {
void World::dump(serial::Writer &writer) const {
  using serial::serialize;
  serialize<std::uint8_t>(writer, _humanoids.size());
  for (const auto &humanoid : _humanoids) {
    serialize<std::uint32_t>(writer, humanoid->get_network_id());
    serialize<float>(writer, humanoid->get_position().x());
    serialize<float>(writer, humanoid->get_position().y());
    serialize<float>(writer, humanoid->get_position().z());
    serialize<Humanoid_input_state>(writer, humanoid->get_input_state());
  }
  serialize<std::uint16_t>(writer, _projectiles.size());
  for (const auto &projectile : _projectiles) {
    serialize<std::uint32_t>(writer, projectile->get_network_id());
    serialize<float>(writer, projectile->get_position().x());
    serialize<float>(writer, projectile->get_position().y());
    serialize<float>(writer, projectile->get_position().z());
    serialize<float>(writer, projectile->get_velocity().x());
    serialize<float>(writer, projectile->get_velocity().y());
    serialize<float>(writer, projectile->get_velocity().z());
  }
}

bool World::add(const rc::Strong<Player> &player) {
  const auto it = std::ranges::find(_players, player);
  if (it == _players.end()) {
    _players.emplace_back(player);
    return true;
  } else {
    return false;
  }
}

bool World::remove(const rc::Strong<Player> &player) noexcept {
  const auto it = std::ranges::find(_players, player);
  if (it != _players.end()) {
    on_remove(**it);
    algorithms::unordered_erase_at(_players, it);
    return true;
  } else {
    return false;
  }
}

bool World::add(const rc::Strong<Humanoid> &humanoid) {
  const auto it = std::ranges::find(_humanoids, humanoid);
  if (it == _humanoids.end()) {
    _humanoids.emplace_back(humanoid);
    return true;
  } else {
    return false;
  }
}

bool World::remove(const rc::Strong<Humanoid> &humanoid) noexcept {
  const auto it = std::ranges::find(_humanoids, humanoid);
  if (it != _humanoids.end()) {
    on_remove(**it);
    algorithms::unordered_erase_at(_humanoids, it);
    return true;
  } else {
    return false;
  }
}

bool World::add(const rc::Strong<Projectile> &projectile) {
  const auto it = std::ranges::find(_projectiles, projectile);
  if (it == _projectiles.end()) {
    _projectiles.emplace_back(projectile);
    return true;
  } else {
    return false;
  }
}

bool World::remove(const rc::Strong<Projectile> &projectile) noexcept {
  const auto it = std::ranges::find(_projectiles, projectile);
  if (it != _projectiles.end()) {
    on_remove(**it);
    algorithms::unordered_erase_at(_projectiles, it);
    return true;
  } else {
    return false;
  }
}

std::pmr::vector<rc::Strong<Player>>
World::get_players(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<rc::Strong<Player>>{memory_resource};
  retval.assign_range(_players);
  return retval;
}

std::pmr::vector<rc::Strong<Humanoid>>
World::get_humanoids(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<rc::Strong<Humanoid>>{memory_resource};
  retval.assign_range(_humanoids);
  return retval;
}

std::pmr::vector<rc::Strong<Projectile>>
World::get_projectiles(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<rc::Strong<Projectile>>{memory_resource};
  retval.assign_range(_projectiles);
  return retval;
}

void World::on_remove(Game_object &game_object) {
  for (const auto &remove_listener : game_object._removal_listeners) {
    remove_listener->on_remove_game_object();
  }
  game_object.on_remove();
}

std::size_t World::get_player_count() const noexcept { return _players.size(); }

std::size_t World::get_humanoid_count() const noexcept {
  return _humanoids.size();
}

std::size_t World::get_projectile_count() const noexcept {
  return _projectiles.size();
}
} // namespace fpsparty::game
