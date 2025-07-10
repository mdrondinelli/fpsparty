#include "humanoid.hpp"

namespace fpsparty::game_replica {
std::uint32_t Humanoid::get_network_id() const noexcept { return _network_id; }

const game_core::Humanoid_input_state &
Humanoid::get_input_state() const noexcept {
  return _input_state;
}

void Humanoid::set_input_state(
    const game_core::Humanoid_input_state &value) noexcept {
  _input_state = value;
}

const Eigen::Vector3f &Humanoid::get_position() const noexcept {
  return _position;
}

void Humanoid::set_position(const Eigen::Vector3f &value) noexcept {
  _position = value;
}
} // namespace fpsparty::game_replica
