#ifndef FPSPARTY_MATH_QUAT_HPP
#define FPSPARTY_MATH_QUAT_HPP

#include <Eigen/Geometry>

#include "flt.hpp"

namespace fpsparty::math {

using f32quat = Eigen::Quaternion<f32>;
using f64quat = Eigen::Quaternion<f64>;

using quat = f32quat;
using dquat = f64quat;

} // namespace fpsparty::math

#endif // FPSPARTY_MATH_QUAT_HPP
