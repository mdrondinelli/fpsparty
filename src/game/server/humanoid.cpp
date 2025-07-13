#include "humanoid.hpp"
#include "game/core/game_object_id.hpp"

namespace fpsparty::game {
Humanoid::Humanoid(Game_object_id game_object_id,
                   const Humanoid_create_info &) noexcept
    : Game_object{game_object_id} {}

void Humanoid::on_remove() {}

const Humanoid_input_state &Humanoid::get_input_state() const noexcept {
  return _input_state;
}

void Humanoid::set_input_state(const Humanoid_input_state &value) noexcept {
  _input_state = value;
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

void Humanoid::set_position(const Eigen::Vector3f &value) noexcept {
  _position = value;
}

const Eigen::Vector3f &Humanoid::get_velocity() const noexcept {
  return _velocity;
}

void Humanoid::set_velocity(const Eigen::Vector3f &value) noexcept {
  _velocity = value;
}
} // namespace fpsparty::game
