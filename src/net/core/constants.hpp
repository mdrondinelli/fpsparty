#ifndef FPSPARTY_NET_CORE_CONSTANTS_HPP
#define FPSPARTY_NET_CORE_CONSTANTS_HPP

#include <cstddef>
#include <cstdint>

namespace fpsparty::net::constants {
constexpr auto port = std::uint16_t{1109};
constexpr auto max_clients = std::size_t{32};
constexpr auto max_channels = std::size_t{3};
constexpr auto player_initialization_channel_id = std::uint8_t{0};
constexpr auto player_input_state_channel_id = std::uint8_t{1};
constexpr auto entity_snapshot_channel_id = std::uint8_t{1};
constexpr auto grid_snapshot_channel_id = std::uint8_t{2};
} // namespace fpsparty::net::constants

#endif
