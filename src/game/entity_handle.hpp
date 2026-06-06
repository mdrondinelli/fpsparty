#ifndef FPSPARTY_GAME_ENTITY_HANDLE_HPP
#define FPSPARTY_GAME_ENTITY_HANDLE_HPP

#include <net/core/entity_id.hpp>

namespace fpsparty::game {

template <typename EntityType> struct Entity_handle {
  net::Entity_id id;

  constexpr operator bool() const noexcept { return id != 0; }
};

} // namespace fpsparty::game

#endif
