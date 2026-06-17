#include "game.hpp"

#include <algorithm>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <math/transforms.hpp>

#include "constants.hpp"
#include "humanoid.hpp"
#include "item.hpp"
#include "player.hpp"

namespace fpsparty::game {
namespace {
void handle_use_secondary(Grid &grid, Humanoid &humanoid) {
  auto const basis = (math::y_rotation_matrix(humanoid.curr_input_state.yaw) *
                      math::x_rotation_matrix(humanoid.curr_input_state.pitch))
                       .eval();
  auto const forward = basis.col(2).head<3>().eval();
  auto const origin =
    (humanoid.position + math::vec3::UnitY() * Humanoid::eye_height).eval();
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
  _entities.register_entity_type<Item>();
}

void Game::tick(float duration) {
  // copy humanoid curr input state -> prev input state
  for (auto [humanoid, _] : _entities.get_all<Humanoid>()) {
    humanoid.prev_input_state = humanoid.curr_input_state;
  }
  // copy player input state -> humanoid curr input state, spawn humanoids
  auto const should_spawn = true; //_entities.get_all<Humanoid>().size() < 2;
  for (auto [player, _] : _entities.get_all<Player>()) {
    auto humanoid = _entities.get(player.humanoid);
    if (player.humanoid && !humanoid) {
      player.humanoid = {};
    }
    if (!player.humanoid && should_spawn) {
      auto const humanoid_entry = _entities.emplace<Humanoid>();
      player.humanoid = humanoid_entry.handle;
      humanoid = &humanoid_entry.entity;
      humanoid->position = math::vec3{0.0f, 4.0f, 0.0f};
    }
    if (humanoid) {
      humanoid->curr_input_state = player.input_state;
    }
  }
  // humnanoid movement and primary/secondary
  for (auto [humanoid, humanoid_handle] : _entities.get_all<Humanoid>()) {
    if (humanoid.grounded) {
      humanoid.velocity.y() = 0.0f;
      auto const desired_direction = humanoid.world_movement_direction();
      auto const desired_speed = humanoid.curr_input_state.run
                                   ? Humanoid::run_speed
                                   : Humanoid::walk_speed;
      auto const desired_velocity = (desired_direction * desired_speed).eval();
      auto const delta_velocity = (desired_velocity - humanoid.velocity).eval();
      // max acceleration is applied when going
      humanoid.velocity +=
        std::min(delta_velocity.norm() / (2.0f * desired_speed), 1.0f) *
        Humanoid::ground_acceleration * delta_velocity.normalized() * duration;
      if (humanoid.curr_input_state.jump && !humanoid.prev_input_state.jump) {
        humanoid.velocity += Humanoid::jump_speed * math::vec3::UnitY();
      }
    } else {
      auto const desired_direction = humanoid.world_movement_direction();
      humanoid.velocity += desired_direction * Humanoid::air_acceleration * duration;
    }
    humanoid.integrate(duration);
    humanoid.grounded = false;
    auto constexpr humanoid_contact_iterations = 3;
    for (auto i = 0; i < humanoid_contact_iterations; ++i) {
      if (auto const contact = _grid.find_contact(humanoid.bounds())) {
        humanoid.position -= contact->normal.cast<f32>() * contact->separation;
        if (contact->normal.x() > 0) {
          humanoid.velocity.x() = std::max(humanoid.velocity.x(), 0.0f);
        } else if (contact->normal.y() > 0) {
          humanoid.velocity.y() = std::max(humanoid.velocity.y(), 0.0f);
          humanoid.grounded = true;
        } else if (contact->normal.z() > 0) {
          humanoid.velocity.z() = std::max(humanoid.velocity.z(), 0.0f);
        } else if (contact->normal.x() < 0) {
          humanoid.velocity.x() = std::min(humanoid.velocity.x(), 0.0f);
        } else if (contact->normal.y() < 0) {
          humanoid.velocity.y() = std::min(humanoid.velocity.y(), 0.0f);
        } else if (contact->normal.z() < 0) {
          humanoid.velocity.z() = std::min(humanoid.velocity.z(), 0.0f);
        }
      } else {
        break;
      }
    }
    if (
      humanoid.curr_input_state.use_primary && humanoid.attack_timer == 0.0f) {
      auto const basis =
        (math::y_rotation_matrix(humanoid.curr_input_state.yaw) *
         math::x_rotation_matrix(humanoid.curr_input_state.pitch))
          .eval();
      auto const up = basis.col(1).template head<3>().eval();
      auto const forward = basis.col(2).template head<3>().eval();
      _entities.insert<Item>({
        .creator = humanoid_handle,
        .position = humanoid.position + Eigen::Vector3f::UnitY() * 1.5f,
        .velocity = humanoid.velocity +
                    constants::item_forward_speed * forward +
                    constants::item_up_speed * up,
      });
      humanoid.attack_timer = Humanoid::attack_cooldown;
    }
    if (
      !humanoid.prev_input_state.use_secondary &&
      humanoid.curr_input_state.use_secondary) {
      handle_use_secondary(_grid, humanoid);
    }
  }
  // accumulate item forces
  auto items = _entities.get_all<Item>();
  for (auto outer = items.begin(); outer != items.end(); ++outer) {
    auto &outer_item = outer.entity();
    for (auto inner = outer + 1; inner != items.end(); ++inner) {
      auto &inner_item = inner.entity();
      auto const displacement =
        (inner_item.position - outer_item.position).eval();
      auto const distance = displacement.norm();
      auto const separation = distance - 2.0f * Item::half_extent;
      if (separation < 0.0f) {
        auto const n = distance > 0.0f ? (displacement / distance).eval()
                                       : math::vec3::UnitX().eval();
        auto const inner_item_normal_velocity = inner_item.velocity.dot(n);
        auto const outer_item_normal_velocity = outer_item.velocity.dot(n);
        auto const v_rel_n =
          inner_item_normal_velocity - outer_item_normal_velocity;
        auto const f = (Item::repulsion_stiffness * -separation * n -
                        Item::repulsion_damping * v_rel_n * n)
                         .eval();
        inner_item.force += f;
        outer_item.force -= f;
      }
    }
  }
  // item movement and collision
  auto const item_subtick_count = 3;
  for (auto subtick = 0; subtick < item_subtick_count; ++subtick) {
    for (auto it = items.begin(); it != items.end();) {
      auto &item = it.entity();
      auto const position_before = item.position;
      item.integrate(duration / item_subtick_count);
      if (item.position.y() < -64.0f) {
        it = items.erase(it);
        continue;
      }
      auto const contact = _grid.find_contact(item.bounds());
      if (contact) {
        auto const normal = contact->normal.cast<f32>().eval();
        auto const normal_positional_impulse =
          (Item::mass * normal * -contact->separation).eval();
        item.position += normal_positional_impulse / Item::mass;
        auto const position_after = item.position;
        auto const position_delta = (position_after - position_before).eval();
        auto const normal_position_delta = position_delta.dot(normal);
        auto const tangent_position_delta =
          (position_delta - normal_position_delta * normal).eval();
        auto const friction_positional_impulse_norm =
          Item::mass * Item::friction * -contact->separation;
        if (
          friction_positional_impulse_norm < Item::mass * tangent_position_delta
                                                            .norm()) {
          auto const friction_positional_impulse =
            (friction_positional_impulse_norm * -tangent_position_delta
                                                   .normalized())
              .eval();
          item.position += friction_positional_impulse / Item::mass;
        } else {
          item.position -= tangent_position_delta;
        }
        auto const momentum_before = (Item::mass * item.velocity).eval();
        if (contact->normal.x() > 0) {
          item.velocity.x() = std::max(item.velocity.x(), 0.0f);
        } else if (contact->normal.x() < 0) {
          item.velocity.x() = std::min(item.velocity.x(), 0.0f);
        } else if (contact->normal.y() > 0) {
          item.velocity.y() = std::max(item.velocity.y(), 0.0f);
        } else if (contact->normal.y() < 0) {
          item.velocity.y() = std::min(item.velocity.y(), 0.0f);
        } else if (contact->normal.z() > 0) {
          item.velocity.z() = std::max(item.velocity.z(), 0.0f);
        } else if (contact->normal.z() < 0) {
          item.velocity.z() = std::min(item.velocity.z(), 0.0f);
        }
        auto const momentum_after = (Item::mass * item.velocity).eval();
        if (!momentum_after.isZero()) {
          auto const normal_impulse = (momentum_after - momentum_before).eval();
          auto const frictional_impulse_norm =
            Item::friction * normal_impulse.norm();
          if (frictional_impulse_norm < momentum_after.norm()) {
            auto const frictional_impulse =
              (frictional_impulse_norm * -item.velocity.normalized()).eval();
            item.velocity += frictional_impulse / Item::mass;
          } else {
            item.velocity = math::vec3::Zero();
          }
        }
      }
      ++it;
    }
  }
  // clear item forces
  for (auto [item, _] : _entities.get_all<Item>()) {
    item.force = math::vec3::Zero();
  }
  // humanoid-item collision
  for (auto [humanoid, humanoid_handle] : _entities.get_all<Humanoid>()) {
    auto const humanoid_bounds = humanoid.bounds();
    for (auto item_it = items.begin(); item_it != items.end();) {
      auto &item = item_it.entity();
      if (
        item.creator == humanoid_handle &&
        item.age < Item::self_pickup_cooldown) {
        ++item_it;
        continue;
      }
      if (item.bounds().intersects(humanoid_bounds)) {
        item_it = items.erase(item_it);
        continue;
      }
      ++item_it;
    }
  }
}

Grid const &Game::get_grid() const noexcept { return _grid; }

Grid &Game::get_grid() noexcept { return _grid; }

Entity_world const &Game::get_entities() const noexcept { return _entities; }

Entity_world &Game::get_entities() noexcept { return _entities; }

} // namespace fpsparty::game
