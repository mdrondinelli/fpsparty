#ifndef FPSPARTY_GAME_PROJECTILE_HPP
#define FPSPARTY_GAME_PROJECTILE_HPP

#include <math/vec.hpp>
#include <serial/writer.hpp>

#include "entity_handle.hpp"
#include "entity_traits.hpp"
#include "entity_type.hpp"

namespace fpsparty::game {

struct Humanoid;

struct Projectile {
  static auto constexpr half_extent = 0.125f;

  Entity_handle<Humanoid> creator{};
  math::vec3 position{math::vec3::Zero()};
  math::vec3 velocity{math::vec3::Zero()};
};

template <> struct Entity_traits<Projectile> {
  static constexpr Entity_type type = Entity_type::projectile;

  static void dump(serial::Writer &writer, Projectile const &entity);
};

} // namespace fpsparty::game

#endif
