#ifndef FPSPARTY_MATH_BOX_HPP
#define FPSPARTY_MATH_BOX_HPP

#include <Eigen/Geometry>

#include <flt.hpp>
#include <int.hpp>

namespace fpsparty::math {

using f32box2 = Eigen::AlignedBox<f32, 2>;
using f32box3 = Eigen::AlignedBox<f32, 3>;
using f64box2 = Eigen::AlignedBox<f64, 2>;
using f64box3 = Eigen::AlignedBox<f64, 3>;
using i32box2 = Eigen::AlignedBox<i32, 2>;
using i32box3 = Eigen::AlignedBox<i32, 3>;

using box2 = f32box2;
using box3 = f32box3;
using dbox2 = f64box2;
using dbox3 = f64box3;
using ibox2 = i32box2;
using ibox3 = i32box3;

} // namespace fpsparty::math

#endif
