#include "game.hpp"
#include "game/core/constants.hpp"
#include "game/core/humanoid_movement.hpp"
#include "game/core/projectile_movement.hpp"
#include "game/core/sequence_number.hpp"
#include "math/transformation_matrices.hpp"
#include "rc.hpp"
#include <Eigen/Dense>
#include <Eigen/Geometry>

namespace fpsparty::game {
Game::Game(const Game_create_info &) {}

void Game::tick(float duration) {
  const auto players = _world.get_players();
  if (_world.get_humanoid_count() < 2) {
    for (const auto &player : players) {
      auto humanoid = player->get_humanoid().lock();
      if (!humanoid) {
        humanoid = create_humanoid({});
        _world.add(humanoid);
        player->set_humanoid(humanoid);
      }
    }
  }
  for (const auto &player : players) {
    if (const auto player_humanoid = player->get_humanoid().lock()) {
      player_humanoid->set_input_state(player->get_input_state());
    }
  }
  const auto humanoids = _world.get_humanoids();
  for (const auto &humanoid : humanoids) {
    humanoid->decrease_attack_cooldown(duration);
    if (humanoid->get_input_state().use_primary &&
        humanoid->get_attack_cooldown() == 0.0f) {
      const auto basis =
          (math::y_rotation_matrix(humanoid->get_input_state().yaw) *
           math::x_rotation_matrix(humanoid->get_input_state().pitch))
              .eval();
      const auto up = basis.col(1).head<3>().eval();
      const auto forward = basis.col(2).head<3>().eval();
      _world.add(create_projectile({
          .creator = humanoid,
          .position =
              humanoid->get_position() + Eigen::Vector3f::UnitY() * 1.5f,
          .velocity = humanoid->get_velocity() +
                      constants::projectile_forward_speed * forward +
                      constants::projectile_up_speed * up,
      }));
      humanoid->set_attack_cooldown(constants::attack_cooldown);
    }
    const auto movement_info = Humanoid_movement_simulation_info{
        .initial_position = humanoid->get_position(),
        .input_state = humanoid->get_input_state(),
        .duration = duration,
    };
    const auto movement_result = simulate_humanoid_movement(movement_info);
    humanoid->set_position(movement_result.final_position);
    humanoid->set_velocity(
        (movement_result.final_position - movement_info.initial_position) /
        duration);
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
  for (const auto &humanoid : humanoids) {
    const auto humanoid_bounds = Eigen::AlignedBox3f{
        humanoid->get_position() -
            Eigen::Vector3f{
                constants::humanoid_half_width,
                0.0f,
                constants::humanoid_half_width,
            },
        humanoid->get_position() +
            Eigen::Vector3f{
                constants::humanoid_half_width,
                constants::humanoid_height,
                constants::humanoid_half_width,
            },
    };
    for (const auto &projectile : projectiles) {
      if (projectile->get_creator() == humanoid) {
        continue;
      }
      const auto projectile_half_extents = Eigen::Vector3f{
          constants::projectile_half_extent,
          constants::projectile_half_extent,
          constants::projectile_half_extent,
      };
      const auto projectile_bounds = Eigen::AlignedBox3f{
          projectile->get_position() - projectile_half_extents,
          projectile->get_position() + projectile_half_extents,
      };
      if (projectile_bounds.intersects(humanoid_bounds)) {
        _world.remove(humanoid);
        break;
      }
    }
  }
  for (const auto &projectile : projectiles) {
    if (projectile->get_position().y() < 0.0f) {
      _world.remove(projectile);
    }
  }
  ++_tick_number;
}

rc::Strong<Player> Game::create_player(const Player_create_info &info) {
  return _player_factory.create(_next_entity_id++, info);
}

rc::Strong<Humanoid> Game::create_humanoid(const Humanoid_create_info &info) {
  return _humanoid_factory.create(_next_entity_id++, info);
}

rc::Strong<Projectile>
Game::create_projectile(const Projectile_create_info &info) {
  return _projectile_factory.create(_next_entity_id++, info);
}

Sequence_number Game::get_tick_number() const noexcept { return _tick_number; }

const World &Game::get_world() const noexcept { return _world; }

World &Game::get_world() noexcept { return _world; }
} // namespace fpsparty::game
