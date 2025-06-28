#include "game.hpp"

namespace fpsparty::game {
struct Humanoid::Impl {
  std::uint32_t network_id;
  game_core::Humanoid_input_state input_state{};
  std::optional<std::uint16_t> input_sequence_number{};
  bool input_fresh{};
  float attack_cooldown{};
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
};

std::uint32_t Humanoid::get_network_id() const noexcept {
  return _impl->network_id;
}

game_core::Humanoid_input_state Humanoid::get_input_state() const noexcept {
  return _impl->input_state;
}

void Humanoid::set_input_state(
    const game_core::Humanoid_input_state &input_state,
    std::uint16_t input_sequence_number, bool input_fresh) const noexcept {
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
Humanoid::get_input_sequence_number() const noexcept {
  return _impl->input_sequence_number;
}

void Humanoid::increment_input_sequence_number() const noexcept {
  if (_impl->input_sequence_number) {
    ++*_impl->input_sequence_number;
  }
}

bool Humanoid::is_input_stale() const noexcept { return !_impl->input_fresh; }

void Humanoid::mark_input_stale() const noexcept { _impl->input_fresh = false; }

float Humanoid::get_attack_cooldown() const noexcept {
  return _impl->attack_cooldown;
}

void Humanoid::set_attack_cooldown(float amount) const noexcept {
  _impl->attack_cooldown = amount;
}

void Humanoid::decrease_attack_cooldown(float amount) const noexcept {
  _impl->attack_cooldown = std::max(_impl->attack_cooldown - amount, 0.0f);
}

const Eigen::Vector3f &Humanoid::get_position() const noexcept {
  return _impl->position;
}

void Humanoid::set_position(const Eigen::Vector3f &position) const noexcept {
  _impl->position = position;
}

const Eigen::Vector3f &Humanoid::get_velocity() const noexcept {
  return _impl->velocity;
}

void Humanoid::set_velocity(const Eigen::Vector3f &velocity) const noexcept {
  _impl->velocity = velocity;
}

Humanoid::Impl *Humanoid::new_impl(std::uint32_t network_id) {
  return new Humanoid::Impl{.network_id = network_id};
}

void Humanoid::delete_impl(Impl *impl) { delete impl; }
} // namespace fpsparty::game
