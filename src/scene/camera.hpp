#ifndef FPSPARTY_SCENE_CAMERA_HPP
#define FPSPARTY_SCENE_CAMERA_HPP

#include <Eigen/Dense>

namespace fpsparty::scene {

struct Camera {
  Eigen::Vector3f position;
  float yaw{};
  float pitch{};
};

} // namespace fpsparty::scene

#endif
