#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include "game/entity_world.hpp"
#include "game/grid.hpp"
#include "game/humanoid.hpp"
#include "game/player.hpp"
#include "game/projectile.hpp"
#include "net/core/entity_id.hpp"

namespace fpsparty::game {
struct Game_create_info {
  Grid_create_info grid_info{};
};

class Game {
  struct Impl;

public:
  explicit Game(Game_create_info const &info);

  void tick(float duration);

  Entity_owner<Player> create_player(Player_create_info const &info);

  Entity_owner<Humanoid> create_humanoid(Humanoid_create_info const &info);

  Entity_owner<Projectile>
  create_projectile(Projectile_create_info const &info);

  Grid const &get_grid() const noexcept;

  Grid &get_grid() noexcept;

  Entity_world const &get_entities() const noexcept;

  Entity_world &get_entities() noexcept;

private:
  Entity_factory<Player> _player_factory{};
  Entity_factory<Humanoid> _humanoid_factory{};
  Entity_factory<Projectile> _projectile_factory{};
  Grid _grid;
  Entity_world _entities{};
  net::Entity_id _next_entity_id{1};
};
} // namespace fpsparty::game

#endif
