#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include "game/core/entity_id.hpp"
#include "game/core/grid.hpp"
#include "game/core/sequence_number.hpp"
#include "game/server/entity_world.hpp"
#include "game/server/humanoid.hpp"
#include "game/server/player.hpp"
#include "game/server/projectile.hpp"
#include <Eigen/Dense>
#include <exception>

namespace fpsparty::game {
struct Game_create_info {
  Grid_create_info grid_info{};
};

class Game_snapshotting_error : public std::exception {};

class Game {
  struct Impl;

public:
  explicit Game(const Game_create_info &info);

  void tick(float duration);

  rc::Strong<Player> create_player(const Player_create_info &info);

  rc::Strong<Humanoid> create_humanoid(const Humanoid_create_info &info);

  rc::Strong<Projectile> create_projectile(const Projectile_create_info &info);

  Sequence_number get_tick_number() const noexcept;

  const Grid &get_grid() const noexcept;

  Grid &get_grid() noexcept;

  const Entity_world &get_entities() const noexcept;

  Entity_world &get_entities() noexcept;

private:
  rc::Factory<Player> _player_factory{};
  rc::Factory<Humanoid> _humanoid_factory{};
  rc::Factory<Projectile> _projectile_factory{};
  Sequence_number _tick_number{};
  Grid _grid;
  Entity_world _entities{};
  Entity_id _next_entity_id{1};
};
} // namespace fpsparty::game

#endif
