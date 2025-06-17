#ifndef FPSPARTY_CLIENT_TRANSFORMATION_MATRICES_HPP
#define FPSPARTY_CLIENT_TRANSFORMATION_MATRICES_HPP

#include <Eigen/Dense>

namespace fpsparty::client {
constexpr Eigen::Matrix4f translation_matrix(const Eigen::Vector3f &t) {
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
  retval(0, 3) = t.x();
  retval(1, 3) = t.y();
  retval(2, 3) = t.z();
  return retval;
}

constexpr Eigen::Matrix4f perspective_projection_matrix(float zoom_x,
                                                        float zoom_y,
                                                        float z_near) noexcept {
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Zero()};
  retval(0, 0) = -1.0f / zoom_x;
  retval(1, 1) = -1.0f / zoom_y;
  retval(2, 2) = 1.0f;
  retval(2, 3) = -z_near;
  retval(3, 2) = 1.0f;
  return retval;
}
} // namespace fpsparty::client

#endif
