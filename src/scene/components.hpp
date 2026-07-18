#ifndef FPSPARTY_SCENE_COMPONENTS_HPP
#define FPSPARTY_SCENE_COMPONENTS_HPP

#include <math/vec.hpp>
#include <net/entity_id.hpp>

namespace fpsparty::scene::components {

struct Humanoid {
  net::Entity_id entity_id;
  math::vec3 position;
  float pitch{};
  float yaw{};
};

struct Item {
  net::Entity_id entity_id;
  math::vec3 position;
};

} // namespace fpsparty::scene::components

#endif
