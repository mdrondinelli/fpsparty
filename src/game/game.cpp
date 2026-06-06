#include "game.hpp"

#include "game/constants.hpp"
#include "game/humanoid_movement.hpp"
#include "game/projectile_movement.hpp"
#include "math/transformation_matrices.hpp"

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <algorithm>
#include <utility>
#include <vector>

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

template <typename EntityType>
void erase_unique(
  Entity_world &world, std::vector<Entity_handle<EntityType>> &handles) {
  std::ranges::sort(handles, {}, &Entity_handle<EntityType>::id);
  auto const duplicate_begin = std::ranges::unique(handles).begin();
  handles.erase(duplicate_begin, handles.end());
  for (auto const handle : handles) {
    if (world.get_entity(handle)) {
      world.erase_entity(handle);
    }
  }
  handles.clear();
}
} // namespace

Game::Game(Game_create_info const &info) : _grid{info.grid_info} {}

void Game::tick(float duration) {
  for (auto &humanoid : _entities.get_entities_of_type<Humanoid>()) {
    humanoid.prev_input_state = humanoid.curr_input_state;
  }

  for (auto &player : _entities.get_entities_of_type<Player>()) {
    auto *humanoid = _entities.get_entity(player.humanoid);
    if (player.humanoid && !humanoid) {
      player.humanoid = {};
    }
    if (
      !player.humanoid &&
      _entities.get_entities_of_type<Humanoid>().size() < 2) {
      player.humanoid = create_humanoid();
      humanoid = _entities.get_entity(player.humanoid);
    }
    if (humanoid) {
      humanoid->curr_input_state = player.input_state;
    }
  }

  for (auto [humanoid, humanoid_handle] :
       _entities.get_entities_with_handles<Humanoid>()) {
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
      create_projectile({
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

  auto projectile_removals = std::vector<Entity_handle<Projectile>>{};
  for (auto [projectile, projectile_handle] :
       _entities.get_entities_with_handles<Projectile>()) {
    if (projectile.creator && !_entities.get_entity(projectile.creator)) {
      projectile.creator = {};
    }

    auto const movement_result = simulate_projectile_movement({
      .initial_position = projectile.position,
      .initial_velocity = projectile.velocity,
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
      projectile_removals.push_back(projectile_handle);
      continue;
    }
    projectile.position = movement_result.final_position;
    projectile.velocity = movement_result.final_velocity;
  }
  erase_unique(_entities, projectile_removals);

  auto humanoid_removals = std::vector<Entity_handle<Humanoid>>{};
  for (auto [humanoid, humanoid_handle] :
       _entities.get_entities_with_handles<Humanoid>()) {
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
    for (auto &projectile : _entities.get_entities_of_type<Projectile>()) {
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
        humanoid_removals.push_back(humanoid_handle);
        break;
      }
    }
  }
  erase_unique(_entities, humanoid_removals);

  for (auto [projectile, projectile_handle] :
       _entities.get_entities_with_handles<Projectile>()) {
    if (projectile.position.y() < 0.0f) {
      projectile_removals.push_back(projectile_handle);
    }
  }
  erase_unique(_entities, projectile_removals);
}

Entity_handle<Player> Game::create_player(Player player) {
  return _entities.emplace_entity<Player>(std::move(player)).second;
}

Entity_handle<Humanoid> Game::create_humanoid(Humanoid humanoid) {
  return _entities.emplace_entity<Humanoid>(std::move(humanoid)).second;
}

Entity_handle<Projectile> Game::create_projectile(Projectile projectile) {
  return _entities.emplace_entity<Projectile>(std::move(projectile)).second;
}

Grid const &Game::get_grid() const noexcept { return _grid; }

Grid &Game::get_grid() noexcept { return _grid; }

Entity_world const &Game::get_entities() const noexcept { return _entities; }

Entity_world &Game::get_entities() noexcept { return _entities; }

} // namespace fpsparty::game
