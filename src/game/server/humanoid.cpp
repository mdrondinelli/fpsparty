#include "humanoid.hpp"

namespace fpsparty::game {
Humanoid::Humanoid(std::uint32_t network_id,
                   const Humanoid_create_info &) noexcept
    : _network_id{network_id} {}

void Humanoid::on_remove() {}

std::uint32_t Humanoid::get_network_id() const noexcept { return _network_id; }

const Humanoid_input_state &Humanoid::get_input_state() const noexcept {
  return _input_state;
}

void Humanoid::set_input_state(
    const Humanoid_input_state &input_state) noexcept {
  _input_state = input_state;
}

float Humanoid::get_attack_cooldown() const noexcept {
  return _attack_cooldown;
}

void Humanoid::set_attack_cooldown(float amount) noexcept {
  _attack_cooldown = amount;
}

void Humanoid::decrease_attack_cooldown(float amount) noexcept {
  _attack_cooldown = std::max(_attack_cooldown - amount, 0.0f);
}

const Eigen::Vector3f &Humanoid::get_position() const noexcept {
  return _position;
}

void Humanoid::set_position(const Eigen::Vector3f &position) noexcept {
  _position = position;
}

const Eigen::Vector3f &Humanoid::get_velocity() const noexcept {
  return _velocity;
}

void Humanoid::set_velocity(const Eigen::Vector3f &velocity) noexcept {
  _velocity = velocity;
}
} // namespace fpsparty::game
