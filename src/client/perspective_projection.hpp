#ifndef FPSPARTY_CLIENT_PERSPECTIVE_PROJECTION_HPP
#define FPSPARTY_CLIENT_PERSPECTIVE_PROJECTION_HPP

#include <Eigen/Dense>

namespace fpsparty::client {
constexpr Eigen::Matrix4f
make_perspective_projection_matrix(float zoom_x, float zoom_y,
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
