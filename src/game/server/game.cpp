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
Game::Game(Game_create_info const &info) : _grid{info.grid_info} {}

void Game::tick(float duration) {
  auto const players = _entities.get_entities_of_type<Player>();
  if (_entities.count_entities_of_type<Humanoid>() < 2) {
    for (auto const &player : players) {
      if (!player->get_humanoid()) {
        auto humanoid = create_humanoid({});
        auto const humanoid_raw = humanoid.get();
        _entities.add(std::move(humanoid));
        player->set_humanoid(humanoid_raw);
      }
    }
  }
  for (auto const &player : players) {
    if (auto const humanoid = player->get_humanoid()) {
      humanoid->set_input_state(player->get_input_state());
    }
  }
  auto const humanoids = _entities.get_entities_of_type<Humanoid>();
  for (auto const &humanoid : humanoids) {
    humanoid->decrease_attack_cooldown(duration);
    if (
      humanoid->get_input_state().use_primary &&
      humanoid->get_attack_cooldown() == 0.0f) {
      auto const basis =
        (math::y_rotation_matrix(humanoid->get_input_state().yaw) *
         math::x_rotation_matrix(humanoid->get_input_state().pitch))
          .eval();
      auto const up = basis.col(1).head<3>().eval();
      auto const forward = basis.col(2).head<3>().eval();
      _entities.add(create_projectile({
        .creator = humanoid,
        .position = humanoid->get_position() + Eigen::Vector3f::UnitY() * 1.5f,
        .velocity = humanoid->get_velocity() +
                    constants::projectile_forward_speed * forward +
                    constants::projectile_up_speed * up,
      }));
      humanoid->set_attack_cooldown(constants::attack_cooldown);
    }
    auto const movement_info = Humanoid_movement_simulation_info{
      .initial_position = humanoid->get_position(),
      .input_state = humanoid->get_input_state(),
      .duration = duration,
    };
    auto const movement_result = simulate_humanoid_movement(movement_info);
    humanoid->set_position(movement_result.final_position);
    humanoid->set_velocity(
      (movement_result.final_position - movement_info.initial_position) /
      duration);
  }
  auto const projectiles = _entities.get_entities_of_type<Projectile>();
  for (auto const &projectile : projectiles) {
    auto const movement_result = simulate_projectile_movement({
      .initial_position = projectile->get_position(),
      .initial_velocity = projectile->get_velocity(),
      .duration = duration,
    });
    projectile->set_position(movement_result.final_position);
    projectile->set_velocity(movement_result.final_velocity);
  }
  for (auto const &humanoid : humanoids) {
    auto const humanoid_bounds = Eigen::AlignedBox3f{
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
    for (auto const &projectile : projectiles) {
      if (projectile->get_creator() == humanoid) {
        continue;
      }
      auto const projectile_half_extents = Eigen::Vector3f{
        constants::projectile_half_extent,
        constants::projectile_half_extent,
        constants::projectile_half_extent,
      };
      auto const projectile_bounds = Eigen::AlignedBox3f{
        projectile->get_position() - projectile_half_extents,
        projectile->get_position() + projectile_half_extents,
      };
      if (projectile_bounds.intersects(humanoid_bounds)) {
        _entities.remove(humanoid);
        break;
      }
    }
  }
  for (auto const &projectile : projectiles) {
    if (projectile->get_position().y() < 0.0f) {
      _entities.remove(projectile);
    }
  }
  ++_tick_number;
}

Entity_owner<Player> Game::create_player(Player_create_info const &info) {
  return _player_factory.create(_next_entity_id++, info);
}

Entity_owner<Humanoid> Game::create_humanoid(Humanoid_create_info const &info) {
  return _humanoid_factory.create(_next_entity_id++, info);
}

Entity_owner<Projectile>
Game::create_projectile(Projectile_create_info const &info) {
  return _projectile_factory.create(_next_entity_id++, info);
}

net::Sequence_number Game::get_tick_number() const noexcept {
  return _tick_number;
}

Grid const &Game::get_grid() const noexcept { return _grid; }

Grid &Game::get_grid() noexcept { return _grid; }

Entity_world const &Game::get_entities() const noexcept { return _entities; }

Entity_world &Game::get_entities() noexcept { return _entities; }
} // namespace fpsparty::game
