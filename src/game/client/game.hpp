#ifndef FPSPARTY_GAME_REPLICATED_GAME_HPP
#define FPSPARTY_GAME_REPLICATED_GAME_HPP

#include "game_replica/humanoid.hpp"
#include "game_replica/player.hpp"
#include "game_replica/projectile.hpp"
#include "rc.hpp"
#include "serial/reader.hpp"
#include <exception>
#include <memory_resource>
#include <vector>

namespace fpsparty::game_replica {
struct Game_create_info {};

struct Game_simulate_info {
  float duration;
};

class Game_snapshot_application_error : std::exception {};

class Game {
public:
  explicit Game(const Game_create_info &info);

  void clear();

  void simulate(const Game_simulate_info &info);

  void load_world_state(serial::Reader &reader);

  void load_player_state(serial::Reader &reader);

  rc::Strong<Player>
  get_player_by_network_id(std::uint32_t network_id) const noexcept;

  rc::Strong<Humanoid>
  get_humanoid_by_network_id(std::uint32_t network_id) const noexcept;

  rc::Strong<Projectile>
  get_projectile_by_network_id(std::uint32_t network_id) const noexcept;

private:
  std::unique_ptr<std::pmr::memory_resource> _player_memory{};
  std::unique_ptr<std::pmr::memory_resource> _humanoid_memory{};
  std::unique_ptr<std::pmr::memory_resource> _projectile_memory{};
  rc::Factory<Player> _player_factory{};
  rc::Factory<Humanoid> _humanoid_factory{};
  rc::Factory<Projectile> _projectile_factory{};
  std::vector<rc::Strong<Player>> _world_players{};
  std::vector<rc::Strong<Humanoid>> _world_humanoids{};
  std::vector<rc::Strong<Projectile>> _world_projectiles{};
};
} // namespace fpsparty::game_replica

#endif
