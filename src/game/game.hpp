#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include "game/entity_world.hpp"
#include "game/grid.hpp"
#include "game/humanoid.hpp"
#include "game/player.hpp"
#include "game/projectile.hpp"
namespace fpsparty::game {
struct Game_create_info {
  Grid_create_info grid_info{};
};

class Game {
  struct Impl;

public:
  explicit Game(Game_create_info const &info);

  void tick(float duration);

  Entity_handle<Player> create_player(Player player = {});

  Entity_handle<Humanoid> create_humanoid(Humanoid humanoid = {});

  Entity_handle<Projectile> create_projectile(Projectile projectile = {});

  Grid const &get_grid() const noexcept;

  Grid &get_grid() noexcept;

  Entity_world const &get_entities() const noexcept;

  Entity_world &get_entities() noexcept;

private:
  Grid _grid;
  Entity_world _entities{};
};
} // namespace fpsparty::game

#endif
