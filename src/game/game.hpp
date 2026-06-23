#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include "game/entity_world.hpp"
#include "game/grid.hpp"

namespace fpsparty::game {
struct Game_create_info {
  math::ibox3 grid_bounds{};
};

class Game {
  struct Impl;

public:
  explicit Game(Game_create_info const &info);

  void tick(float duration);

  Grid const &get_grid() const noexcept;

  Grid &get_grid() noexcept;

  Entity_world const &get_entities() const noexcept;

  Entity_world &get_entities() noexcept;

private:
  Block_bounds_registry _block_bounds_registry;
  Grid _grid;
  Entity_world _entities{};
};
} // namespace fpsparty::game

#endif
