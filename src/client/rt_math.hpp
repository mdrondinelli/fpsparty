#ifndef FPSPARTY_CLIENT_RT_MATH_HPP
#define FPSPARTY_CLIENT_RT_MATH_HPP

#include "math/mat.hpp"
#include "math/transforms.hpp"
#include "scene/elements.hpp"
#include <array>

namespace fpsparty::client {

// Row-major top 3 rows of a mat4, as pushed to Rt_matrix in shaders.
inline std::array<float, 12> make_rt_matrix_rows(math::mat4 const &matrix) {
  auto rows = std::array<float, 12>{};
  for (auto row = 0; row != 3; ++row) {
    for (auto column = 0; column != 4; ++column) {
      rows[static_cast<std::size_t>(row * 4 + column)] = matrix(row, column);
    }
  }
  return rows;
}

inline math::mat4 make_model_matrix(scene::elements::Box const &box) {
  auto rotation = math::mat4::Identity().eval();
  rotation.block<3, 3>(0, 0) = box.orientation.toRotationMatrix();
  return (math::translation_matrix(box.position) * rotation *
          math::axis_aligned_scale_matrix(box.half_extents * 2.0f))
    .eval();
}

} // namespace fpsparty::client

#endif
