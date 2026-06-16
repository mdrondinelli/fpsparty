#include "projectile.hpp"

#include <serial/serialize.hpp>

#include "gravity.hpp"

namespace fpsparty::game {

void Projectile::integrate(float dt) {
  auto const acceleration = force / mass;
  velocity -= math::vec3::UnitY() * constants::gravitational_acceleration * dt;
  velocity += acceleration * dt;
  position += velocity * dt;
}

void Entity_traits<Projectile>::dump(
  serial::Writer &writer, Projectile const &projectile) {
  using serial::serialize;
  serialize<Eigen::Vector3f>(writer, projectile.position);
  serialize<Eigen::Vector3f>(writer, projectile.velocity);
}

} // namespace fpsparty::game
