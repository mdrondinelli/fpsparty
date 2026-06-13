#include "game.hpp"

#include <algorithm>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include "game/constants.hpp"
#include "game/humanoid.hpp"
#include "game/humanoid_movement.hpp"
#include "game/player.hpp"
#include "game/projectile.hpp"
#include "game/projectile_movement.hpp"
#include "math/transformation_matrices.hpp"

namespace fpsparty::game {
namespace {
void handle_use_secondary(Grid &grid, Humanoid &humanoid) {
  auto const basis = (math::y_rotation_matrix(humanoid.curr_input_state.yaw) *
                      math::x_rotation_matrix(humanoid.curr_input_state.pitch))
                       .eval();
  auto const forward = basis.col(2).head<3>().eval();
  auto const origin = (humanoid.position + Eigen::Vector3f::UnitY() *
                                             constants::humanoid_eye_height)
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

Game::Game(Game_create_info const &info) : _grid{info.grid_info} {
  _entities.register_entity_type<Player>();
  _entities.register_entity_type<Humanoid>();
  _entities.register_entity_type<Projectile>();
}

void Game::tick(float duration) {
  // copy humanoid curr input state -> prev input state
  for (auto [humanoid, _] : _entities.get_all<Humanoid>()) {
    humanoid.prev_input_state = humanoid.curr_input_state;
  }
  // copy player input state -> humanoid curr input state, spawn humanoids
  auto const should_spawn = _entities.get_all<Humanoid>().size() < 2;
  for (auto [player, _] : _entities.get_all<Player>()) {
    auto humanoid = _entities.get(player.humanoid);
    if (player.humanoid && !humanoid) {
      player.humanoid = {};
    }
    if (!player.humanoid && should_spawn) {
      player.humanoid = _entities.emplace<Humanoid>().handle;
      humanoid = _entities.get(player.humanoid);
    }
    if (humanoid) {
      humanoid->curr_input_state = player.input_state;
    }
  }
  // humnanoid movement and primary/secondary
  for (auto [humanoid, humanoid_handle] : _entities.get_all<Humanoid>()) {
    if (
      !humanoid.prev_input_state.use_secondary &&
      humanoid.curr_input_state.use_secondary) {
      handle_use_secondary(_grid, humanoid);
    }
    humanoid.attack_cooldown =
      std::max(0.0f, humanoid.attack_cooldown - duration);
    if (
      humanoid.curr_input_state.use_primary &&
      humanoid.attack_cooldown == 0.0f) {
      auto const basis =
        (math::y_rotation_matrix(humanoid.curr_input_state.yaw) *
         math::x_rotation_matrix(humanoid.curr_input_state.pitch))
          .eval();
      auto const up = basis.col(1).template head<3>().eval();
      auto const forward = basis.col(2).template head<3>().eval();
      _entities.insert<Projectile>({
        .creator = humanoid_handle,
        .position = humanoid.position + Eigen::Vector3f::UnitY() * 1.5f,
        .velocity = humanoid.velocity +
                    constants::projectile_forward_speed * forward +
                    constants::projectile_up_speed * up,
      });
      humanoid.attack_cooldown = constants::attack_cooldown;
    }
    auto const movement_info = Humanoid_movement_simulation_info{
      .initial_position = humanoid.position,
      .input_state = humanoid.curr_input_state,
      .duration = duration,
    };
    auto const movement_result = simulate_humanoid_movement(movement_info);
    humanoid.velocity =
      (movement_result.final_position - movement_info.initial_position) /
      duration;
    humanoid.position = movement_result.final_position;
  }
  // projectile movement, collision with grid, y < 0
  auto projectiles = _entities.get_all<Projectile>();
  for (auto it = projectiles.begin(); it != projectiles.end();) {
    auto &projectile = it.entity();
    auto const movement_result = simulate_projectile_movement({
      .initial_position = projectile.position,
      .initial_velocity = projectile.velocity,
      .duration = duration,
    });
    if (movement_result.final_position.y() < 0.0f) {
      it = projectiles.erase(it);
      continue;
    }
    auto const projectile_cell_indices =
      (movement_result.final_position / constants::grid_cell_stride)
        .array()
        .floor()
        .matrix()
        .cast<int>()
        .eval();
    if (_grid.is_solid(projectile_cell_indices)) {
      _grid.set_solid(projectile_cell_indices, false);
      it = projectiles.erase(it);
      continue;
    }
    projectile.position = movement_result.final_position;
    projectile.velocity = movement_result.final_velocity;
    ++it;
  }
  // humanoid-projectile collision
  auto humanoids = _entities.get_all<Humanoid>();
  for (auto it = humanoids.begin(); it != humanoids.end();) {
    auto [humanoid, humanoid_handle] = *it;
    auto erased = false;
    auto const humanoid_bounds = Eigen::AlignedBox3f{
      humanoid.position -
        Eigen::Vector3f{
          constants::humanoid_half_width,
          0.0f,
          constants::humanoid_half_width,
        },
      humanoid.position +
        Eigen::Vector3f{
          constants::humanoid_half_width,
          constants::humanoid_height,
          constants::humanoid_half_width,
        },
    };
    for (auto [projectile, _] : _entities.get_all<Projectile>()) {
      if (projectile.creator == humanoid_handle) {
        continue;
      }
      auto const projectile_half_extents =
        Eigen::Vector3f::Constant(constants::projectile_half_extent);
      auto const projectile_bounds = Eigen::AlignedBox3f{
        projectile.position - projectile_half_extents,
        projectile.position + projectile_half_extents,
      };
      if (projectile_bounds.intersects(humanoid_bounds)) {
        it = humanoids.erase(it);
        erased = true;
        break;
      }
    }
    if (!erased) {
      ++it;
    }
  }
}

Grid const &Game::get_grid() const noexcept { return _grid; }

Grid &Game::get_grid() noexcept { return _grid; }

Entity_world const &Game::get_entities() const noexcept { return _entities; }

Entity_world &Game::get_entities() noexcept { return _entities; }

} // namespace fpsparty::game
