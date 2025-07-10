#include "projectile.hpp"

namespace fpsparty::game_replica {
std::uint32_t Projectile::get_network_id() const noexcept {
  return _network_id;
}

const Eigen::Vector3f &Projectile::get_position() const noexcept {
  return _position;
}

void Projectile::set_position(const Eigen::Vector3f &value) noexcept {
  _position = value;
}

const Eigen::Vector3f &Projectile::get_velocity() const noexcept {
  return _velocity;
}

void Projectile::set_velocity(const Eigen::Vector3f &value) noexcept {
  _velocity = value;
}
} // namespace fpsparty::game_replica
