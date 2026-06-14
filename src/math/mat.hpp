#ifndef FPSPARTY_MATH_MAT_HPP
#define FPSPARTY_MATH_MAT_HPP

#include <Eigen/Dense>

#include "flt.hpp"
#include "int.hpp"

namespace fpsparty::math {

using f32mat4 = Eigen::Matrix<f32, 4, 4>;
using f64mat4 = Eigen::Matrix<f64, 4, 4>;
using i32mat4 = Eigen::Matrix<i32, 4, 4>;
using u32mat4 = Eigen::Matrix<u32, 4, 4>;

using mat4 = f32mat4;
using dmat4 = f64mat4;
using imat4 = i32mat4;
using umat4 = u32mat4;

}

#endif // FPSPARTY_MATH_MAT_HPP
