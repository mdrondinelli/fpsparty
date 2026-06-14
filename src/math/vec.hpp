#ifndef FPSPARTY_MATH_VECTOR_HPP
#define FPSPARTY_MATH_VECTOR_HPP

#include <Eigen/Dense>

#include <flt.hpp>
#include <int.hpp>

namespace fpsparty::math {

using f32vec2 = Eigen::Matrix<f32, 2, 1>;
using f32vec3 = Eigen::Matrix<f32, 2, 1>;
using f32vec4 = Eigen::Matrix<f32, 4, 1>;
using f64vec2 = Eigen::Matrix<f64, 2, 1>;
using f64vec3 = Eigen::Matrix<f64, 2, 1>;
using f64vec4 = Eigen::Matrix<f64, 4, 1>;
using i8vec2 = Eigen::Matrix<i8, 2, 1>;
using i8vec3 = Eigen::Matrix<i8, 3, 1>;
using i8vec4 = Eigen::Matrix<i8, 4, 1>;
using i16vec2 = Eigen::Matrix<i16, 2, 1>;
using i16vec3 = Eigen::Matrix<i16, 3, 1>;
using i16vec4 = Eigen::Matrix<i16, 4, 1>;
using i32vec2 = Eigen::Matrix<i32, 2, 1>;
using i32vec3 = Eigen::Matrix<i32, 3, 1>;
using i32vec4 = Eigen::Matrix<i32, 4, 1>;
using i64vec2 = Eigen::Matrix<i64, 2, 1>;
using i64vec3 = Eigen::Matrix<i64, 3, 1>;
using i64vec4 = Eigen::Matrix<i64, 4, 1>;
using u8vec2 = Eigen::Matrix<u8, 2, 1>;
using u8vec3 = Eigen::Matrix<u8, 3, 1>;
using u8vec4 = Eigen::Matrix<u8, 4, 1>;
using u16vec2 = Eigen::Matrix<u16, 2, 1>;
using u16vec3 = Eigen::Matrix<u16, 3, 1>;
using u16vec4 = Eigen::Matrix<u16, 4, 1>;
using u32vec2 = Eigen::Matrix<u32, 2, 1>;
using u32vec3 = Eigen::Matrix<u32, 3, 1>;
using u32vec4 = Eigen::Matrix<u32, 4, 1>;
using u64vec2 = Eigen::Matrix<u64, 2, 1>;
using u64vec3 = Eigen::Matrix<u64, 3, 1>;
using u64vec4 = Eigen::Matrix<u64, 4, 1>;

using vec2 = f32vec2;
using vec3 = f32vec3;
using vec4 = f32vec4;

using dvec2 = f64vec2;
using dvec3 = f64vec3;
using dvec4 = f64vec4;

using ivec2 = i32vec2;
using ivec3 = i32vec3;
using ivec4 = i32vec4;

using uvec2 = u32vec2;
using uvec3 = u32vec3;
using uvec4 = u32vec4;

} // namespace fpsparty

#endif
