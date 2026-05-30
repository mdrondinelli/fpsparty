#include "replicated_game.hpp"
#include "game/client/replicated_humanoid.hpp"
#include "game/client/replicated_player.hpp"
#include "game/client/replicated_projectile.hpp"
#include "game/core/entity_world.hpp"
#include "game/core/humanoid_movement.hpp"
#include "game/core/projectile_movement.hpp"

namespace fpsparty::game {
Replicated_game::Replicated_game(Replicated_game_create_info const &) {}

void Replicated_game::tick(float duration) {
  auto const players = _entities.get_entities_of_type<Replicated_player>();
  for (auto const &player : players) {
    if (auto const humanoid = player->get_humanoid()) {
      humanoid->set_input_state(player->get_input_state());
    }
  }
  auto const humanoids = _entities.get_entities_of_type<Replicated_humanoid>();
  for (auto const &humanoid : humanoids) {
    auto const movement_result = simulate_humanoid_movement({
      .initial_position = humanoid->get_position(),
      .input_state = humanoid->get_input_state(),
      .duration = duration,
    });
    humanoid->set_position(movement_result.final_position);
  }
  auto const projectiles =
    _entities.get_entities_of_type<Replicated_projectile>();
  for (auto const &projectile : projectiles) {
    auto const movement_result = simulate_projectile_movement({
      .initial_position = projectile->get_position(),
      .initial_velocity = projectile->get_velocity(),
      .duration = duration,
    });
    projectile->set_position(movement_result.final_position);
    projectile->set_velocity(movement_result.final_velocity);
  }
}

void Replicated_game::load_grid(Replicated_game_grid_load_info const &info) {
  _grid.load(*info.reader);
}

void Replicated_game::load_entities(
  Replicated_game_entities_load_info const &info) {
  using serial::deserialize;
  _tick_number = info.tick_number;
  auto const readers = std::array<serial::Reader *, 2>{
    info.public_state_reader,
    info.player_state_reader,
  };
  auto const loaders = std::array<Entity_loader *, 3>{
    &_humanoid_loader,
    &_projectile_loader,
    &_player_loader,
  };
  _entities.load({
    .readers = readers,
    .loaders = loaders,
  });
}

void Replicated_game::reset() {
  _entities.reset();
  _tick_number = 0;
}

Grid const &Replicated_game::get_grid() const noexcept { return _grid; }

Grid &Replicated_game::get_grid() noexcept { return _grid; }

Entity_world const &Replicated_game::get_entities() const noexcept {
  return _entities;
}

Entity_world &Replicated_game::get_entities() noexcept { return _entities; }

std::uint64_t Replicated_game::get_tick_number() const noexcept {
  return _tick_number;
}
} // namespace fpsparty::game
