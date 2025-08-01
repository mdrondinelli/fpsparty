#ifndef FPSPARTY_GAME_CLIENT_REPLICATED_GAME_HPP
#define FPSPARTY_GAME_CLIENT_REPLICATED_GAME_HPP

#include "game/client/replicated_humanoid.hpp"
#include "game/client/replicated_player.hpp"
#include "game/client/replicated_projectile.hpp"
#include "game/core/entity_world.hpp"
#include "game/core/grid.hpp"
#include "net/core/sequence_number.hpp"
#include "serial/reader.hpp"
#include <exception>

namespace fpsparty::game {
struct Replicated_game_create_info {};

struct Replicated_game_grid_load_info {
  serial::Reader *reader{};
};

struct Replicated_game_entities_load_info {
  net::Sequence_number tick_number{};
  serial::Reader *public_state_reader{};
  serial::Reader *player_state_reader{};
};

class Replicated_game_entities_load_error : public std::exception {};

class Replicated_game {
public:
  explicit Replicated_game(const Replicated_game_create_info &info);

  void tick(float duration);

  void load_grid(const Replicated_game_grid_load_info &info);

  void load_entities(const Replicated_game_entities_load_info &info);

  void reset();

  const Entity_world &get_entities() const noexcept;

  Entity_world &get_entities() noexcept;

  net::Sequence_number get_tick_number() const noexcept;

private:
  Grid _grid{{}};
  Replicated_humanoid_loader _humanoid_loader{};
  Replicated_projectile_loader _projectile_loader{};
  Replicated_player_loader _player_loader{};
  Entity_world _entities{};
  net::Sequence_number _tick_number{};
};
} // namespace fpsparty::game

#endif
