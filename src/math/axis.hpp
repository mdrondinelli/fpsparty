#ifndef FPSPARTY_MATH_AXIS_HPP
#define FPSPARTY_MATH_AXIS_HPP

#include <math/vec.hpp>

#include "sign.hpp"

namespace fpsparty::math {

enum class axis2 { x, y };

enum class axis3 { x, y, z };

struct signed_axis2 {
  axis2 axis;
  sign sign;

  constexpr u64 index() const noexcept {
    return static_cast<u64>(axis) << 1 | static_cast<u64>(sign);
  }

  operator math::vec2() const noexcept {
    if (axis == axis2::x) {
      if (sign == sign::positive) {
        return math::vec2::UnitX();
      } else {
        return -math::vec2::UnitX();
      }
    } else {
      if (sign == sign::positive) {
        return math::vec2::UnitY();
      } else {
        return -math::vec2::UnitY();
      }
    }
  }

  operator math::ivec2() const noexcept {
    if (axis == axis2::x) {
      if (sign == sign::positive) {
        return math::ivec2::UnitX();
      } else {
        return -math::ivec2::UnitX();
      }
    } else {
      if (sign == sign::positive) {
        return math::ivec2::UnitY();
      } else {
        return -math::ivec2::UnitY();
      }
    }
  }
};

struct signed_axis3 {
  signed_axis3 cross(signed_axis3 other) const noexcept {
    auto const n = static_cast<math::ivec3>(*this)
                     .cross(static_cast<math::ivec3>(other))
                     .eval();
    if (n.x() == 1) {
      return {axis3::x, sign::positive};
    } else if (n.x() == -1) {
      return {axis3::x, sign::negative};
    } else if (n.y() == 1) {
      return {axis3::y, sign::positive};
    } else if (n.y() == -1) {
      return {axis3::y, sign::negative};
    } else if (n.z() == 1) {
      return {axis3::z, sign::positive};
    } else /*if (n.z() == -1)*/ {
      return {axis3::z, sign::negative};
    }
  }

  constexpr u64 index() const noexcept {
    return static_cast<u64>(axis) << 1 | static_cast<u64>(sign);
  }

  operator math::vec3() const noexcept {
    if (axis == axis3::x) {
      if (sign == sign::positive) {
        return math::vec3::UnitX();
      } else {
        return -math::vec3::UnitX();
      }
    } else if (axis == axis3::y) {
      if (sign == sign::positive) {
        return math::vec3::UnitY();
      } else {
        return -math::vec3::UnitY();
      }
    } else {
      if (sign == sign::positive) {
        return math::vec3::UnitZ();
      } else {
        return -math::vec3::UnitZ();
      }
    }
  }

  operator math::ivec3() const noexcept {
    if (axis == axis3::x) {
      if (sign == sign::positive) {
        return math::ivec3::UnitX();
      } else {
        return -math::ivec3::UnitX();
      }
    } else if (axis == axis3::y) {
      if (sign == sign::positive) {
        return math::ivec3::UnitY();
      } else {
        return -math::ivec3::UnitY();
      }
    } else {
      if (sign == sign::positive) {
        return math::ivec3::UnitZ();
      } else {
        return -math::ivec3::UnitZ();
      }
    }
  }

  axis3 axis;
  sign sign;
};

inline signed_axis2 operator+(axis2 axis) noexcept {
  return signed_axis2{axis, sign::positive};
}

inline signed_axis2 operator-(axis2 axis) noexcept {
  return signed_axis2{axis, sign::negative};
}

inline signed_axis2 operator-(signed_axis2 signed_axis) noexcept {
  return signed_axis2{
    signed_axis.axis,
    signed_axis.sign == sign::positive ? sign::negative : sign::positive};
}

inline signed_axis3 operator+(axis3 axis) noexcept {
  return signed_axis3{axis, sign::positive};
}

inline signed_axis3 operator-(axis3 axis) noexcept {
  return signed_axis3{axis, sign::negative};
}

inline signed_axis3 operator-(signed_axis3 signed_axis) noexcept {
  return signed_axis3{
    signed_axis.axis,
    signed_axis.sign == sign::positive ? sign::negative : sign::positive};
}

struct scaled_axis2 {
  axis2 axis;
  float scale;

  explicit operator signed_axis2() const noexcept {
    return signed_axis2{axis, scale >= 0.0f ? sign::positive : sign::negative};
  }

  operator math::vec2() const noexcept { return math::vec2{+axis} * scale; }
};

struct scaled_axis3 {
  axis3 axis;
  float scale;

  explicit operator signed_axis3() const noexcept {
    return signed_axis3{axis, scale >= 0.0f ? sign::positive : sign::negative};
  }

  operator math::vec3() const noexcept { return math::vec3{+axis} * scale; }
};

inline scaled_axis2 operator*(float scale, axis2 axis) noexcept {
  return scaled_axis2{axis, scale};
}

inline scaled_axis2 operator*(axis2 axis, float scale) noexcept {
  return scaled_axis2{axis, scale};
}

inline scaled_axis3 operator*(float scale, axis3 axis) noexcept {
  return scaled_axis3{axis, scale};
}

inline scaled_axis3 operator*(axis3 axis, float scale) noexcept {
  return scaled_axis3{axis, scale};
}

} // namespace fpsparty::math

#endif
