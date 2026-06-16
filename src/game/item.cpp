#include "item.hpp"

#include <serial/serialize.hpp>

#include "gravity.hpp"

namespace fpsparty::game {

void Item::integrate(float dt) {
  auto const acceleration = force / mass;
  velocity -= math::vec3::UnitY() * constants::gravitational_acceleration * dt;
  velocity += acceleration * dt;
  position += velocity * dt;
}

void Entity_traits<Item>::dump(serial::Writer &writer, Item const &item) {
  using serial::serialize;
  serialize<Eigen::Vector3f>(writer, item.position);
}

} // namespace fpsparty::game
