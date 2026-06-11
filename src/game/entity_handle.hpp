#ifndef FPSPARTY_GAME_ENTITY_HANDLE_HPP
#define FPSPARTY_GAME_ENTITY_HANDLE_HPP

#include <net/entity_id.hpp>

#include <concepts>
#include <type_traits>

namespace fpsparty::game {

template <typename EntityType> struct Entity_handle {
  constexpr Entity_handle() noexcept = default;

  constexpr explicit Entity_handle(net::Entity_id id) noexcept : id{id} {}

  template <typename OtherEntityType>
    requires(
      std::is_const_v<EntityType> &&
      std::same_as<std::remove_const_t<EntityType>, OtherEntityType>)
  constexpr Entity_handle(Entity_handle<OtherEntityType> other) noexcept
      : id{other.id} {}

  constexpr operator bool() const noexcept { return id != 0; }

  friend constexpr bool
  operator==(Entity_handle const &, Entity_handle const &) noexcept = default;

  net::Entity_id id{};
};

} // namespace fpsparty::game

#endif
