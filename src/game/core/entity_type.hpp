#ifndef FPSPARTY_GAME_ENTITY_TYPE_HPP
#define FPSPARTY_GAME_ENTITY_TYPE_HPP

#include <cstdint>

namespace fpsparty::game {
enum class Entity_type : std::uint8_t {
  humanoid,
  player,
  projectile,
};
}

#endif
