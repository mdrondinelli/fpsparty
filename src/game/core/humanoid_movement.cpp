#include "humanoid_movement.hpp"
#include "game/core/constants.hpp"
#include "math/transformation_matrices.hpp"

namespace fpsparty::game {
Humanoid_movement_simulation_result
simulate_humanoid_movement(const Humanoid_movement_simulation_info &info) {
  const auto basis_matrix = math::y_rotation_matrix(info.input_state.yaw);
  auto movement_vector = Eigen::Vector3f{0.0f, 0.0f, 0.0f};
  if (info.input_state.move_left) {
    movement_vector += basis_matrix.col(0).head<3>();
  }
  if (info.input_state.move_right) {
    movement_vector -= basis_matrix.col(0).head<3>();
  }
  if (info.input_state.move_forward) {
    movement_vector += basis_matrix.col(2).head<3>();
  }
  if (info.input_state.move_backward) {
    movement_vector -= basis_matrix.col(2).head<3>();
  }
  movement_vector.normalize();
  return {
    .final_position = info.initial_position + movement_vector *
                                                constants::humanoid_speed *
                                                info.duration,
  };
}
} // namespace fpsparty::game
