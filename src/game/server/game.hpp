#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include "game/core/game_object_id.hpp"
#include "game/core/sequence_number.hpp"
#include "game/server/humanoid.hpp"
#include "game/server/player.hpp"
#include "game/server/projectile.hpp"
#include "game/server/world.hpp"
#include <Eigen/Dense>
#include <exception>

namespace fpsparty::game {
struct Game_create_info {};

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

  const World &get_world() const noexcept;

  World &get_world() noexcept;

private:
  rc::Factory<Player> _player_factory{};
  rc::Factory<Humanoid> _humanoid_factory{};
  rc::Factory<Projectile> _projectile_factory{};
  Sequence_number _tick_number{};
  Game_object_id _next_network_id{1};
  World _world{};
};
} // namespace fpsparty::game

#endif
