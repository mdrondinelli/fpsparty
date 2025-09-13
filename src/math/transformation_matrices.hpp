#ifndef FPSPARTY_MATH_TRANSFORMATION_MATRICES_HPP
#define FPSPARTY_MATH_TRANSFORMATION_MATRICES_HPP

#include <Eigen/Dense>
#include <cmath>

namespace fpsparty::math {
constexpr Eigen::Matrix4f translation_matrix(const Eigen::Vector3f &t) {
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
  retval(0, 3) = t.x();
  retval(1, 3) = t.y();
  retval(2, 3) = t.z();
  return retval;
}

constexpr Eigen::Matrix4f x_rotation_matrix(float a) {
  const auto cos_a = std::cos(a);
  const auto sin_a = std::sin(a);
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
  retval(1, 1) = cos_a;
  retval(2, 1) = sin_a;
  retval(1, 2) = -sin_a;
  retval(2, 2) = cos_a;
  return retval;
}

constexpr Eigen::Matrix4f y_rotation_matrix(float a) {
  const auto cos_a = std::cos(a);
  const auto sin_a = std::sin(a);
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
  retval(0, 0) = cos_a;
  retval(2, 0) = -sin_a;
  retval(0, 2) = sin_a;
  retval(2, 2) = cos_a;
  return retval;
}

constexpr Eigen::Matrix4f z_rotation_matrix(float a) {
  const auto cos_a = std::cos(a);
  const auto sin_a = std::sin(a);
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
  retval(0, 0) = cos_a;
  retval(1, 0) = sin_a;
  retval(0, 1) = -sin_a;
  retval(1, 1) = cos_a;
  return retval;
}

constexpr Eigen::Matrix4f uniform_scale_matrix(float s) {
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
  retval(0, 0) = s;
  retval(1, 1) = s;
  retval(2, 2) = s;
  return retval;
}

constexpr Eigen::Matrix4f axis_aligned_scale_matrix(const Eigen::Vector3f &s) {
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
  retval(0, 0) = s[0];
  retval(1, 1) = s[1];
  retval(2, 2) = s[2];
  return retval;
}

constexpr Eigen::Matrix4f perspective_projection_matrix(
  float zoom_x, float zoom_y, float z_near
) noexcept {
  auto retval = Eigen::Matrix4f{Eigen::Matrix4f::Zero()};
  retval(0, 0) = -1.0f / zoom_x;
  retval(1, 1) = -1.0f / zoom_y;
  retval(2, 3) = z_near;
  retval(3, 2) = 1.0f;
  return retval;
}
} // namespace fpsparty::math

#endif
