#ifndef FPSPARTY_GAME_CONSTANTS_HPP
#define FPSPARTY_GAME_CONSTANTS_HPP

namespace fpsparty::game::constants {
constexpr auto gravitational_acceleration = 9.81f;
constexpr auto block_interaction_range = 5.0f;
constexpr auto item_forward_speed = 8.0f;
constexpr auto item_up_speed = 2.0f;
constexpr auto item_half_extent = 0.125f;
constexpr auto grid_cell_stride = 1.0f;
} // namespace fpsparty::game::constants

#endif
