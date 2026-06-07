#include "projectile_movement.hpp"
#include "game/constants.hpp"

namespace fpsparty::game {
Projectile_movement_simulation_result simulate_projectile_movement(
  Projectile_movement_simulation_info const &info) noexcept {
  auto const velocity =
    (info.initial_velocity - Eigen::Vector3f::UnitY() *
                               constants::gravitational_acceleration *
                               info.duration)
      .eval();
  auto const position =
    (info.initial_position + velocity * info.duration).eval();
  return {
    .final_position = position,
    .final_velocity = velocity,
  };
}
} // namespace fpsparty::game
