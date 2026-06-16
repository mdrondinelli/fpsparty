#ifndef FPSPARTY_GAME_ITEM_HPP
#define FPSPARTY_GAME_ITEM_HPP

#include <math/box.hpp>
#include <math/vec.hpp>
#include <serial/writer.hpp>

#include "entity_handle.hpp"
#include "entity_traits.hpp"
#include "entity_type.hpp"

namespace fpsparty::game {

struct Humanoid;

struct Item {
  void integrate(float dt);

  math::box3 bounds() const noexcept {
    return math::box3{
      position - math::vec3::Constant(half_extent),
      position + math::vec3::Constant(half_extent),
    };
  }

  Entity_handle<Humanoid> creator{};
  math::vec3 position{math::vec3::Zero()};
  math::vec3 velocity{math::vec3::Zero()};
  math::vec3 force{math::vec3::Zero()};
  float age{0.0f};

  static auto constexpr half_extent = 0.125f;
  static auto constexpr mass = 1.0f; // 1kg
  static auto constexpr friction = 0.5f;
  static auto constexpr repulsion_stiffness = 512.0f;
  static auto constexpr repulsion_damping = 16.0f;
  static auto constexpr self_pickup_cooldown = 0.5f;
};

template <> struct Entity_traits<Item> {
  static constexpr Entity_type type = Entity_type::item;

  static void dump(serial::Writer &writer, Item const &entity);
};

} // namespace fpsparty::game

#endif
