#ifndef FPSPARTY_GAME_SERVER_GAME_HPP
#define FPSPARTY_GAME_SERVER_GAME_HPP

#include "game/core/entity_world.hpp"
#include "game/core/grid.hpp"
#include "game/server/humanoid.hpp"
#include "game/server/player.hpp"
#include "game/server/projectile.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/sequence_number.hpp"

namespace fpsparty::game {
struct Game_create_info {
  Grid_create_info grid_info{};
};

class Game {
  struct Impl;

public:
  explicit Game(const Game_create_info &info);

  void tick(float duration);

  Entity_owner<Player> create_player(const Player_create_info &info);

  Entity_owner<Humanoid> create_humanoid(const Humanoid_create_info &info);

  Entity_owner<Projectile>
  create_projectile(const Projectile_create_info &info);

  net::Sequence_number get_tick_number() const noexcept;

  const Grid &get_grid() const noexcept;

  Grid &get_grid() noexcept;

  const Entity_world &get_entities() const noexcept;

  Entity_world &get_entities() noexcept;

private:
  Entity_factory<Player> _player_factory{};
  Entity_factory<Humanoid> _humanoid_factory{};
  Entity_factory<Projectile> _projectile_factory{};
  Grid _grid;
  Entity_world _entities{};
  net::Entity_id _next_entity_id{1};
  net::Sequence_number _tick_number{};
};
} // namespace fpsparty::game

#endif
