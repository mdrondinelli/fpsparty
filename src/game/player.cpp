#include "player.hpp"
#include "net/core/entity_id.hpp"
#include "serial/serialize.hpp"

namespace fpsparty::game {

void Entity_traits<Player>::dump(serial::Writer &writer, Player const &player) {
  using serial::serialize;
  serialize<net::Entity_id>(writer, player.humanoid.id);
}

} // namespace fpsparty::game
