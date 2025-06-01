#include <cstddef>
#include <cstdint>

namespace fpsparty::constants {
constexpr auto port = std::uint16_t{1109};
constexpr auto max_clients = std::size_t{32};
constexpr auto tick_rate = 64.0f;
constexpr auto tick_duration = 1.0f / tick_rate;
constexpr auto player_movement_speed = 1.0f;
} // namespace fpsparty::constants
