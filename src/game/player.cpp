#include "game.hpp"

namespace fpsparty::game {
struct Player::Impl {
  std::uint32_t network_id;
  Input_state input_state{};
  std::optional<std::uint16_t> input_sequence_number{};
  bool input_fresh{};
  float attack_cooldown{};
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
};

std::uint32_t Player::get_network_id() const noexcept {
  return _impl->network_id;
}

Player::Input_state Player::get_input_state() const noexcept {
  return _impl->input_state;
}

void Player::set_input_state(const Input_state &input_state,
                             std::uint16_t input_sequence_number,
                             bool input_fresh) const noexcept {
  if (_impl->input_sequence_number) {
    const auto difference = static_cast<std::int16_t>(
        input_sequence_number - *_impl->input_sequence_number);
    if (difference < 0) {
      return;
    }
  }
  _impl->input_state = input_state;
  _impl->input_sequence_number = input_sequence_number;
  _impl->input_fresh = input_fresh;
}

std::optional<std::uint16_t>
Player::get_input_sequence_number() const noexcept {
  return _impl->input_sequence_number;
}

void Player::increment_input_sequence_number() const noexcept {
  if (_impl->input_sequence_number) {
    ++*_impl->input_sequence_number;
  }
}

bool Player::is_input_stale() const noexcept { return !_impl->input_fresh; }

void Player::mark_input_stale() const noexcept { _impl->input_fresh = false; }

float Player::get_attack_cooldown() const noexcept {
  return _impl->attack_cooldown;
}

void Player::set_attack_cooldown(float amount) const noexcept {
  _impl->attack_cooldown = amount;
}

void Player::decrease_attack_cooldown(float amount) const noexcept {
  _impl->attack_cooldown = std::max(_impl->attack_cooldown - amount, 0.0f);
}

const Eigen::Vector3f &Player::get_position() const noexcept {
  return _impl->position;
}

void Player::set_position(const Eigen::Vector3f &position) const noexcept {
  _impl->position = position;
}

const Eigen::Vector3f &Player::get_velocity() const noexcept {
  return _impl->velocity;
}

void Player::set_velocity(const Eigen::Vector3f &velocity) const noexcept {
  _impl->velocity = velocity;
}

Player::Impl *Player::new_impl(std::uint32_t network_id) {
  return new Player::Impl{.network_id = network_id};
}

void Player::delete_impl(Impl *impl) { delete impl; }
} // namespace fpsparty::game
