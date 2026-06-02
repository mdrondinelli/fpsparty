#include "humanoid.hpp"
#include "net/core/entity_id.hpp"

namespace fpsparty::game {
Humanoid::Humanoid(
  net::Entity_id entity_id, Humanoid_create_info const &) noexcept
    : Entity{Entity_type::humanoid, entity_id} {}

void Humanoid::on_remove() {}

net::Input_state const &Humanoid::get_input_state() const noexcept {
  return _input_state;
}

net::Input_state const &Humanoid::get_prev_input_state() const noexcept {
  return _prev_input_state;
}

void Humanoid::set_input_state(net::Input_state const &value) noexcept {
  _prev_input_state = _input_state;
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

Eigen::Vector3f const &Humanoid::get_position() const noexcept {
  return _position;
}

void Humanoid::set_position(Eigen::Vector3f const &value) noexcept {
  _position = value;
}

Eigen::Vector3f const &Humanoid::get_velocity() const noexcept {
  return _velocity;
}

void Humanoid::set_velocity(Eigen::Vector3f const &value) noexcept {
  _velocity = value;
}

Entity_type Humanoid_dumper::get_entity_type() const noexcept {
  return Entity_type::humanoid;
}

void Humanoid_dumper::dump_entity(
  serial::Writer &writer, Entity const &entity) const {
  using serial::serialize;
  if (auto const humanoid = dynamic_cast<Humanoid const *>(&entity)) {
    serialize<Eigen::Vector3f>(writer, humanoid->get_position());
    serialize<net::Input_state>(writer, humanoid->get_input_state());
  }
}
} // namespace fpsparty::game
