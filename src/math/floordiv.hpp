#ifndef FPSPARTY_MATH_FLOORDIV_HPP
#define FPSPARTY_MATH_FLOORDIV_HPP

#include <concepts>

namespace fpsparty::math {
template <std::signed_integral T> constexpr T floordiv(T a, T b) {
  return a / b - (a % b != 0 && (a ^ b) < 0);
}
} // namespace fpsparty::math

#endif
