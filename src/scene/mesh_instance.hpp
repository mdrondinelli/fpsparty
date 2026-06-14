#ifndef FPSPARTY_SCENE_MESH_INSTANCE_HPP
#define FPSPARTY_SCENE_MESH_INSTANCE_HPP

#include <Eigen/Dense>

#include "mesh.hpp"

namespace fpsparty::scene {

struct Mesh_instance {
  Mesh mesh;
  Eigen::Vector3f position;
  Eigen::Quaternionf orientation{Eigen::Quaternionf::Identity()};
  Eigen::Vector3f scale{Eigen::Vector3f::Ones()};
};

} // namespace fpsparty::scene

#endif
