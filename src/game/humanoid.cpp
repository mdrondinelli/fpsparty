#include "humanoid.hpp"

#include <math/transforms.hpp>
#include <math/vec.hpp>
#include <serial/serialize.hpp>

namespace fpsparty::game {

void Humanoid::integrate(float dt) noexcept {
  auto const basis_matrix = math::y_rotation_matrix(curr_input_state.yaw);
  auto movement_direction = math::vec3{0.0f, 0.0f, 0.0f};
  if (curr_input_state.move_left) {
    movement_direction += basis_matrix.col(0).head<3>();
  }
  if (curr_input_state.move_right) {
    movement_direction -= basis_matrix.col(0).head<3>();
  }
  if (curr_input_state.move_forward) {
    movement_direction += basis_matrix.col(2).head<3>();
  }
  if (curr_input_state.move_backward) {
    movement_direction -= basis_matrix.col(2).head<3>();
  }
  if (curr_input_state.jump) {
    movement_direction += Eigen::Vector3f::UnitY();
  }
  if (curr_input_state.crouch) {
    movement_direction -= Eigen::Vector3f::UnitY();
  }
  movement_direction.normalize();
  velocity = movement_direction * movement_speed;
  position += velocity * dt;
  attack_timer -= dt;
  attack_timer = std::max(attack_timer, 0.0f);
}

math::box3 Humanoid::bounds() const noexcept {
  return math::box3{
    position -
      math::vec3{
        half_width,
        0.0f,
        half_width,
      },
    position +
      math::vec3{
        half_width,
        height,
        half_width,
      },
  };
}

void Entity_traits<Humanoid>::dump(
  serial::Writer &writer, Humanoid const &humanoid) {
  using serial::serialize;
  serialize<Eigen::Vector3f>(writer, humanoid.position);
  serialize<net::Input_state>(writer, humanoid.curr_input_state);
  // Note: input state is needed for rendering the facing direction.
}

} // namespace fpsparty::game
