#include "player.hpp"

namespace fpsparty::game_replica {
std::uint32_t Player::get_network_id() const noexcept { return _network_id; }

const rc::Weak<Humanoid> &Player::get_humanoid() const noexcept {
  return _humanoid;
}

void Player::set_humanoid(const rc::Weak<Humanoid> &value) noexcept {
  _humanoid = value;
}

const game_core::Humanoid_input_state &
Player::get_input_state() const noexcept {
  return _input_state;
}

void Player::set_input_state(
    const game_core::Humanoid_input_state &value) noexcept {
  _input_state = value;
}

const std::optional<std::uint16_t> &
Player::get_input_sequence_number() const noexcept {
  return _input_sequence_number;
}

void Player::set_input_sequence_number(
    const std::optional<std::uint16_t> &value) noexcept {
  _input_sequence_number = value;
}
} // namespace fpsparty::game_replica
