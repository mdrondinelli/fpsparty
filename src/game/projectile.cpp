#include "projectile.hpp"

#include "serial/serialize.hpp"

namespace fpsparty::game {

void Entity_traits<Projectile>::dump(
  serial::Writer &writer, Projectile const &projectile) {
  using serial::serialize;
  serialize<Eigen::Vector3f>(writer, projectile.position);
  serialize<Eigen::Vector3f>(writer, projectile.velocity);
}

} // namespace fpsparty::game
