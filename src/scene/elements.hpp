#ifndef FPSPARTY_SCENE_ELEMENTS_HPP
#define FPSPARTY_SCENE_ELEMENTS_HPP

#include <int.hpp>
#include <math/quat.hpp>
#include <math/vec.hpp>
#include <net/entity_id.hpp>

namespace fpsparty::scene::elements {

inline constexpr u64 sun_light_key = 1;

constexpr u64 entity_part_key(net::Entity_id entity_id, u32 part) noexcept {
  return static_cast<u64>(entity_id) << 32 | part;
}

struct Camera {
  u64 key;
  math::vec3 position;
  float pitch{};
  float yaw{};
};

struct Distant_light {
  u64 key;
  math::vec3 direction;
  math::vec3 irradiance;
};

struct Point_light {
  u64 key;
  math::vec3 position;
  math::vec3 irradiance;
};

struct Box {
  u64 key;
  math::vec3 half_extents;
  math::vec3 position;
  math::quat orientation;
};

} // namespace fpsparty::scene::elements

#endif
