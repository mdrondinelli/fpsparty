#ifndef FPSPARTY_MATH_TRANSFORMS_HPP
#define FPSPARTY_MATH_TRANSFORMS_HPP

#include <cmath>

#include "mat.hpp"
#include "vec.hpp"

namespace fpsparty::math {

constexpr mat4 translation_matrix(vec3 t) {
  auto retval = mat4{mat4::Identity()};
  retval(0, 3) = t.x();
  retval(1, 3) = t.y();
  retval(2, 3) = t.z();
  return retval;
}

constexpr mat4 x_rotation_matrix(f32 a) {
  auto const cos_a = std::cos(a);
  auto const sin_a = std::sin(a);
  auto retval = mat4{mat4::Identity()};
  retval(1, 1) = cos_a;
  retval(2, 1) = sin_a;
  retval(1, 2) = -sin_a;
  retval(2, 2) = cos_a;
  return retval;
}

constexpr mat4 y_rotation_matrix(f32 a) {
  auto const cos_a = std::cos(a);
  auto const sin_a = std::sin(a);
  auto retval = mat4{mat4::Identity()};
  retval(0, 0) = cos_a;
  retval(2, 0) = -sin_a;
  retval(0, 2) = sin_a;
  retval(2, 2) = cos_a;
  return retval;
}

constexpr mat4 z_rotation_matrix(f32 a) {
  auto const cos_a = std::cos(a);
  auto const sin_a = std::sin(a);
  auto retval = mat4{mat4::Identity()};
  retval(0, 0) = cos_a;
  retval(1, 0) = sin_a;
  retval(0, 1) = -sin_a;
  retval(1, 1) = cos_a;
  return retval;
}

constexpr mat4 uniform_scale_matrix(f32 s) {
  auto retval = mat4{mat4::Identity()};
  retval(0, 0) = s;
  retval(1, 1) = s;
  retval(2, 2) = s;
  return retval;
}

constexpr mat4 axis_aligned_scale_matrix(vec3 s) {
  auto retval = mat4{mat4::Identity()};
  retval(0, 0) = s[0];
  retval(1, 1) = s[1];
  retval(2, 2) = s[2];
  return retval;
}

constexpr mat4 perspective_projection_matrix(
  float zoom_x, float zoom_y, float z_near) noexcept {
  auto retval = mat4{mat4::Zero()};
  retval(0, 0) = -1.0f / zoom_x;
  retval(1, 1) = -1.0f / zoom_y;
  retval(2, 3) = z_near;
  retval(3, 2) = 1.0f;
  return retval;
}

} // namespace fpsparty::math

#endif
