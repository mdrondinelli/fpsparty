#include "game.hpp"

#include <algorithm>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <math/transforms.hpp>

#include "constants.hpp"
#include "humanoid.hpp"
#include "humanoid_movement.hpp"
#include "player.hpp"
#include "projectile.hpp"

namespace fpsparty::game {
namespace {
void handle_use_secondary(Grid &grid, Humanoid &humanoid) {
  auto const basis = (math::y_rotation_matrix(humanoid.curr_input_state.yaw) *
                      math::x_rotation_matrix(humanoid.curr_input_state.pitch))
                       .eval();
  auto const forward = basis.col(2).head<3>().eval();
  auto const origin =
    (humanoid.position + math::vec3::UnitY() * constants::humanoid_eye_height)
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
    grid.set_solid(hit->cell_coords + hit->normal, true);
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
  // accumulate projectile forces
  auto projectiles = _entities.get_all<Projectile>();
  for (auto outer = projectiles.begin(); outer != projectiles.end(); ++outer) {
    auto &outer_projectile = outer.entity();
    for (auto inner = outer + 1; inner != projectiles.end(); ++inner) {
      auto &inner_projectile = inner.entity();
      auto const displacement =
        (inner_projectile.position - outer_projectile.position).eval();
      auto const distance = displacement.norm();
      auto const separation = distance - 2.0f * Projectile::half_extent;
      if (separation < 0.0f) {
        auto const n = distance > 0.0f ? (displacement / distance).eval()
                                       : math::vec3::UnitX().eval();
        auto const inner_projectile_normal_velocity = inner_projectile.velocity.dot(n);
        auto const outer_projectile_normal_velocity = outer_projectile.velocity.dot(n);
        auto const v_rel_n = inner_projectile_normal_velocity - outer_projectile_normal_velocity;
        auto const f = (Projectile::repulsion_stiffness * -separation * n - Projectile::repulsion_damping * v_rel_n * n).eval();
        inner_projectile.force += f;
        outer_projectile.force -= f;
      }
    }
  }
  // projectile movement and collision
  auto const subtick_count = 2;
  for (auto subtick = 0; subtick < subtick_count; ++subtick) {
    for (auto it = projectiles.begin(); it != projectiles.end();) {
      auto &projectile = it.entity();
      auto const position_before = projectile.position;
      projectile.integrate(duration / subtick_count);
      if (projectile.position.y() < -64.0f) {
        it = projectiles.erase(it);
        continue;
      }
      auto const contact = _grid.find_contact(projectile.bounds());
      if (contact) {
        auto const normal = contact->normal.cast<f32>().eval();
        auto const normal_positional_impulse =
          (Projectile::mass * normal * -contact->separation).eval();
        projectile.position += normal_positional_impulse / Projectile::mass;
        auto const position_after = projectile.position;
        auto const position_delta = (position_after - position_before).eval();
        auto const normal_position_delta = position_delta.dot(normal);
        auto const tangent_position_delta =
          (position_delta - normal_position_delta * normal).eval();
        auto const friction_positional_impulse_norm =
          Projectile::mass * Projectile::friction * -contact->separation;
        if (
          friction_positional_impulse_norm <
          Projectile::mass * tangent_position_delta.norm()) {
          auto const friction_positional_impulse =
            (friction_positional_impulse_norm * -tangent_position_delta
                                                   .normalized())
              .eval();
          projectile.position += friction_positional_impulse / Projectile::mass;
        } else {
          projectile.position -= tangent_position_delta;
        }
        auto const momentum_before =
          (Projectile::mass * projectile.velocity).eval();
        if (contact->normal.x() > 0) {
          projectile.velocity.x() = std::max(projectile.velocity.x(), 0.0f);
        } else if (contact->normal.x() < 0) {
          projectile.velocity.x() = std::min(projectile.velocity.x(), 0.0f);
        } else if (contact->normal.y() > 0) {
          projectile.velocity.y() = std::max(projectile.velocity.y(), 0.0f);
        } else if (contact->normal.y() < 0) {
          projectile.velocity.y() = std::min(projectile.velocity.y(), 0.0f);
        } else if (contact->normal.z() > 0) {
          projectile.velocity.z() = std::max(projectile.velocity.z(), 0.0f);
        } else if (contact->normal.z() < 0) {
          projectile.velocity.z() = std::min(projectile.velocity.z(), 0.0f);
        }
        auto const momentum_after =
          (Projectile::mass * projectile.velocity).eval();
        if (!momentum_after.isZero()) {
          auto const normal_impulse = (momentum_after - momentum_before).eval();
          auto const frictional_impulse_norm =
            Projectile::friction * normal_impulse.norm();
          if (frictional_impulse_norm < momentum_after.norm()) {
            auto const frictional_impulse =
              (frictional_impulse_norm * -projectile.velocity.normalized())
                .eval();
            projectile.velocity += frictional_impulse / Projectile::mass;
          } else {
            projectile.velocity = math::vec3::Zero();
          }
        }
      }
      /*
      auto const projectile_cell_indices =
        (projectile.position / constants::grid_cell_stride)
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
      */
      ++it;
    }
  }
  // clear projectile forces
  for (auto [projectile, _] : _entities.get_all<Projectile>()) {
    projectile.force = math::vec3::Zero();
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
        Eigen::Vector3f::Constant(Projectile::half_extent);
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
