#ifndef FPSPARTY_SCENE_KEYFRAME_HPP
#define FPSPARTY_SCENE_KEYFRAME_HPP

#include <Eigen/Dense>

#include <game/grid.hpp>

#include "camera.hpp"
#include "mesh_instance.hpp"

namespace fpsparty::scene {

template <typename T> struct Identified {
  std::uint64_t id;
  T value;

  Identified() = default;

  Identified(std::uint64_t id, T &&value) noexcept
      : id{id}, value{std::move(value)} {}

  Identified(std::uint64_t id, T const &value) noexcept
      : id{id}, value{value} {}
};

struct Keyframe {
  std::uint64_t number;
  game::Grid grid;
  std::vector<Identified<Camera>> cameras;
  std::vector<Identified<Mesh_instance>> mesh_instances;
  math::vec3 sun_direction;
};

} // namespace fpsparty::scene

#endif
