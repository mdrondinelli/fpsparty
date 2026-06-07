#include "humanoid.hpp"

#include "serial/serialize.hpp"

namespace fpsparty::game {

void Entity_traits<Humanoid>::dump(
  serial::Writer &writer, Humanoid const &humanoid) {
  using serial::serialize;
  serialize<Eigen::Vector3f>(writer, humanoid.position);
  serialize<net::Input_state>(writer, humanoid.curr_input_state);
  // Note: input state is needed for rendering the facing direction.
}

} // namespace fpsparty::game
