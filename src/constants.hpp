#include <cstddef>
#include <cstdint>

namespace fpsparty::constants {
constexpr auto port = std::uint16_t{1109};
constexpr auto max_clients = std::size_t{32};
constexpr auto max_channels = std::size_t{2};
constexpr auto player_id_channel_id = std::uint8_t{0};
constexpr auto player_input_state_channel_id = std::uint8_t{1};
constexpr auto game_state_channel_id = std::uint8_t{1};
constexpr auto tick_rate = 64.0f;
constexpr auto tick_duration = 1.0f / tick_rate;
constexpr auto input_rate = 64.0f;
constexpr auto input_duration = 1.0f / input_rate;
constexpr auto player_movement_speed = 2.0f;
constexpr auto mouselook_sensititvity = 1.0f / 512.0f;
constexpr auto gravitational_acceleration = 9.81f;
constexpr auto attack_cooldown = 1.0f;
} // namespace fpsparty::constants
