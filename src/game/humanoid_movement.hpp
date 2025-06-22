#ifndef FPSPARTY_GAME_HUMANOID_MOVEMENT_HPP
#define FPSPARTY_GAME_HUMANOID_MOVEMENT_HPP

#include "game/game.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
struct Humanoid_movement_simulation_info {
  Eigen::Vector3f initial_position;
  Player::Input_state input_state;
  float duration;
};

struct Humanoid_movement_simulation_result {
  Eigen::Vector3f final_position;
};

Humanoid_movement_simulation_result
simulate_humanoid_movement(const Humanoid_movement_simulation_info &info);
} // namespace fpsparty::game

#endif
