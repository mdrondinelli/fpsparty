#include "replicated_game.hpp"
#include "game/core/humanoid_movement.hpp"
#include "game/core/projectile_movement.hpp"
#include "serial/serialize.hpp"

namespace fpsparty::game {
void Replicated_game::tick(float duration) {
  const auto players = _world.get_players();
  for (const auto &player : players) {
    if (const auto humanoid = player->get_humanoid().lock()) {
      humanoid->set_input_state(player->get_input_state());
    }
  }
  const auto humanoids = _world.get_humanoids();
  for (const auto &humanoid : humanoids) {
    const auto movement_result = simulate_humanoid_movement({
        .initial_position = humanoid->get_position(),
        .input_state = humanoid->get_input_state(),
        .duration = duration,
    });
    humanoid->set_position(movement_result.final_position);
  }
  const auto projectiles = _world.get_projectiles();
  for (const auto &projectile : projectiles) {
    const auto movement_result = simulate_projectile_movement({
        .initial_position = projectile->get_position(),
        .initial_velocity = projectile->get_velocity(),
        .duration = duration,
    });
    projectile->set_position(movement_result.final_position);
    projectile->set_velocity(movement_result.final_velocity);
  }
}

void Replicated_game::load(const Replicated_game_load_info &info) {
  _tick_number = info.tick_number;
  _world.load(*info.world_state_reader);
}

std::uint64_t Replicated_game::get_tick_number() const noexcept {
  return _tick_number;
}

const Replicated_world &Replicated_game::get_world() const noexcept {
  return _world;
}

Replicated_world &Replicated_game::get_world() noexcept { return _world; }
} // namespace fpsparty::game
