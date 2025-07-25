#include "humanoid.hpp"
#include "net/core/entity_id.hpp"

namespace fpsparty::game {
Humanoid::Humanoid(net::Entity_id entity_id,
                   const Humanoid_create_info &) noexcept
    : Entity{Entity_type::humanoid, entity_id} {}

void Humanoid::on_remove() {}

const net::Input_state &Humanoid::get_input_state() const noexcept {
  return _input_state;
}

void Humanoid::set_input_state(const net::Input_state &value) noexcept {
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

Entity_type Humanoid_dumper::get_entity_type() const noexcept {
  return Entity_type::humanoid;
}

void Humanoid_dumper::dump_entity(serial::Writer &writer,
                                  const Entity &entity) const {
  using serial::serialize;
  if (const auto humanoid = dynamic_cast<const Humanoid *>(&entity)) {
    serialize<Eigen::Vector3f>(writer, humanoid->get_position());
    serialize<net::Input_state>(writer, humanoid->get_input_state());
  }
}
} // namespace fpsparty::game
