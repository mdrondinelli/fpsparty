#ifndef FPSPARTY_GAME_CONSTANTS_HPP
#define FPSPARTY_GAME_CONSTANTS_HPP

namespace fpsparty::game::constants {
constexpr auto gravitational_acceleration = 9.81f;
constexpr auto humanoid_speed = 5.0f;
constexpr auto humanoid_half_width = 0.35f;
constexpr auto humanoid_height = 1.8f;
constexpr auto attack_cooldown = 0.8f;
constexpr auto projectile_forward_speed = 15.0f;
constexpr auto projectile_up_speed = 2.0f;
constexpr auto projectile_half_extent = 0.125f;
} // namespace fpsparty::game::constants

#endif
