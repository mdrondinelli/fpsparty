#include "projectile_movement.hpp"
#include "constants.hpp"

namespace fpsparty::game {
Projectile_movement_simulation_result simulate_projectile_movement(
    const Projectile_movement_simulation_info &info) noexcept {
  const auto velocity =
      (info.initial_velocity - Eigen::Vector3f::UnitY() *
                                   constants::gravitational_acceleration *
                                   info.duration)
          .eval();
  const auto position =
      (info.initial_position + velocity * info.duration).eval();
  return {
      .final_position = position,
      .final_velocity = velocity,
  };
}
} // namespace fpsparty::game
