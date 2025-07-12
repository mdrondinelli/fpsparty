#include "replicated_world.hpp"

namespace fpsparty::game {
void Replicated_world::load(serial::Reader &reader) {
  using serial::deserialize;
  const auto humanoid_count = deserialize<std::uint8_t>(reader);
  if (!humanoid_count) {
    throw Replicated_world_load_error{};
  }
  for (auto i = std::uint8_t{}; i != humanoid_count; ++i) {
    const auto network_id = deserialize<std::uint32_t>();
  }
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
