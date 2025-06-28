#ifndef FPSPARTY_GAME_CORE_PROJECTILE_MOVEMENT_HPP
#define FPSPARTY_GAME_CORE_PROJECTILE_MOVEMENT_HPP

#include <Eigen/Dense>

namespace fpsparty::game_core {
struct Projectile_movement_simulation_info {
  Eigen::Vector3f initial_position;
  Eigen::Vector3f initial_velocity;
  float duration;
};

struct Projectile_movement_simulation_result {
  Eigen::Vector3f final_position;
  Eigen::Vector3f final_velocity;
};

Projectile_movement_simulation_result simulate_projectile_movement(
    const Projectile_movement_simulation_info &info) noexcept;
} // namespace fpsparty::game_core

#endif
