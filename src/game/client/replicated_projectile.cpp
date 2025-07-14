#include "replicated_projectile.hpp"
#include "game/core/entity_id.hpp"

namespace fpsparty::game {
Replicated_projectile::Replicated_projectile(
    Entity_id entity_id) noexcept
    : Entity{entity_id} {}

void Replicated_projectile::on_remove() {}

const Eigen::Vector3f &Replicated_projectile::get_position() const noexcept {
  return _position;
}

void Replicated_projectile::set_position(
    const Eigen::Vector3f &value) noexcept {
  _position = value;
}

const Eigen::Vector3f &Replicated_projectile::get_velocity() const noexcept {
  return _velocity;
}

void Replicated_projectile::set_velocity(
    const Eigen::Vector3f &value) noexcept {
  _velocity = value;
}
} // namespace fpsparty::game
