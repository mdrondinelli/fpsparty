#include "game.hpp"
#include "game/core/constants.hpp"
#include "game/core/humanoid_movement.hpp"
#include "game/core/projectile_movement.hpp"
#include "game/server/player.hpp"
#include "math/transformation_matrices.hpp"
#include "net/core/sequence_number.hpp"
#include <Eigen/Dense>
#include <Eigen/Geometry>

namespace fpsparty::game {
Game::Game(const Game_create_info &info) : _grid{info.grid_info} {}

void Game::tick(float duration) {
  const auto players = _entities.get_entities_of_type<Player>();
  if (_entities.count_entities_of_type<Humanoid>() < 2) {
    for (const auto &player : players) {
      if (!player->get_humanoid()) {
        auto humanoid = create_humanoid({});
        const auto humanoid_raw = humanoid.get();
        _entities.add(std::move(humanoid));
        player->set_humanoid(humanoid_raw);
      }
    }
  }
  for (const auto &player : players) {
    if (const auto humanoid = player->get_humanoid()) {
      humanoid->set_input_state(player->get_input_state());
    }
  }
  const auto humanoids = _entities.get_entities_of_type<Humanoid>();
  for (const auto &humanoid : humanoids) {
    humanoid->decrease_attack_cooldown(duration);
    if (
      humanoid->get_input_state().use_primary &&
      humanoid->get_attack_cooldown() == 0.0f) {
      const auto basis =
        (math::y_rotation_matrix(humanoid->get_input_state().yaw) *
         math::x_rotation_matrix(humanoid->get_input_state().pitch))
          .eval();
      const auto up = basis.col(1).head<3>().eval();
      const auto forward = basis.col(2).head<3>().eval();
      _entities.add(create_projectile({
        .creator = humanoid,
        .position = humanoid->get_position() + Eigen::Vector3f::UnitY() * 1.5f,
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
  const auto projectiles = _entities.get_entities_of_type<Projectile>();
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
        _entities.remove(humanoid);
        break;
      }
    }
  }
  for (const auto &projectile : projectiles) {
    if (projectile->get_position().y() < 0.0f) {
      _entities.remove(projectile);
    }
  }
  ++_tick_number;
}

Entity_owner<Player> Game::create_player(const Player_create_info &info) {
  return _player_factory.create(_next_entity_id++, info);
}

Entity_owner<Humanoid> Game::create_humanoid(const Humanoid_create_info &info) {
  return _humanoid_factory.create(_next_entity_id++, info);
}

Entity_owner<Projectile>
Game::create_projectile(const Projectile_create_info &info) {
  return _projectile_factory.create(_next_entity_id++, info);
}

net::Sequence_number Game::get_tick_number() const noexcept {
  return _tick_number;
}

const Grid &Game::get_grid() const noexcept { return _grid; }

Grid &Game::get_grid() noexcept { return _grid; }

const Entity_world &Game::get_entities() const noexcept { return _entities; }

Entity_world &Game::get_entities() noexcept { return _entities; }
} // namespace fpsparty::game
