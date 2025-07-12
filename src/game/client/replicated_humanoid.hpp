#ifndef FPSPARTY_GAME_REPLICATED_HUMANOID_HPP
#define FPSPARTY_GAME_REPLICATED_HUMANOID_HPP

#include "game/core/humanoid_input_state.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
class Replicated_humanoid {
public:
  const Humanoid_input_state &get_input_state() const noexcept;

  void set_input_state(const Humanoid_input_state &value) noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &value) noexcept;

private:
  Humanoid_input_state _input_state{};
  Eigen::Vector3f _position{};
};
} // namespace fpsparty::game

#endif
