#include "game.hpp"
#include "game/constants.hpp"
#include "game/humanoid_movement.hpp"
#include "game/projectile_movement.hpp"
#include "game/player.hpp"
#include "math/transformation_matrices.hpp"
#include <Eigen/Dense>
#include <Eigen/Geometry>

namespace fpsparty::game {
namespace {
void handle_use_secondary(Grid &grid, Humanoid &humanoid) {
  auto const basis =
    (math::y_rotation_matrix(humanoid.get_input_state().yaw) *
     math::x_rotation_matrix(humanoid.get_input_state().pitch))
      .eval();
  auto const forward = basis.col(2).head<3>().eval();
  auto const origin =
    (humanoid.get_position() +
     Eigen::Vector3f::UnitY() * constants::humanoid_eye_height)
      .eval();
  auto const origin_cell =
    (origin / constants::grid_cell_stride).array().floor().matrix().eval();
  auto const origin_cell_indices = origin_cell.cast<int>().eval();
  auto const origin_cell_offset =
    (origin / constants::grid_cell_stride - origin_cell).eval();
  auto const hit = grid.raycast(
    origin_cell_indices,
    origin_cell_offset,
    forward,
    constants::block_interaction_range / constants::grid_cell_stride);
  if (hit) {
    grid.set_solid(hit->cell_indices + hit->normal, true);
  }
}
} // namespace

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
    if (
      !humanoid->get_prev_input_state().use_secondary &&
      humanoid->get_input_state().use_secondary) {
      handle_use_secondary(_grid, *humanoid);
    }
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
    auto const projectile_cell_indices =
      (movement_result.final_position / constants::grid_cell_stride)
        .array()
        .floor()
        .matrix()
        .cast<int>()
        .eval();
    if (_grid.is_solid(projectile_cell_indices)) {
      _grid.set_solid(projectile_cell_indices, false);
      _entities.remove(projectile);
      continue;
    }
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

Grid const &Game::get_grid() const noexcept { return _grid; }

Grid &Game::get_grid() noexcept { return _grid; }

Entity_world const &Game::get_entities() const noexcept { return _entities; }

Entity_world &Game::get_entities() noexcept { return _entities; }
} // namespace fpsparty::game
