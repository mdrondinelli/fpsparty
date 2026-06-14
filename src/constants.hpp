#ifndef FPSPARTY_CONSTANTS_HPP
#define FPSPARTY_CONSTANTS_HPP

#include <cstddef>

namespace fpsparty::constants {
constexpr auto tick_rate = 64.0f;
constexpr auto tick_duration = 1.0f / tick_rate;
constexpr auto mouselook_sensititvity = 1.0f / 512.0f;
} // namespace fpsparty::constants

#endif
