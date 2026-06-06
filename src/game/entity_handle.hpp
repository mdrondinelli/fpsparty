#ifndef FPSPARTY_GAME_ENTITY_HANDLE_HPP
#define FPSPARTY_GAME_ENTITY_HANDLE_HPP

#include <net/core/entity_id.hpp>

namespace fpsparty::game {

template <typename EntityType> struct Entity_handle {
  net::Entity_id id{};

  constexpr operator Entity_handle<const EntityType>() const noexcept {
    return {id};
  }

  constexpr operator bool() const noexcept { return id != 0; }

  friend constexpr bool
  operator==(Entity_handle const &, Entity_handle const &) noexcept = default;
};

} // namespace fpsparty::game

#endif
