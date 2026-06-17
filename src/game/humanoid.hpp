#ifndef FPSPARTY_GAME_HUMANOID_HPP
#define FPSPARTY_GAME_HUMANOID_HPP

#include <math/box.hpp>
#include <math/vec.hpp>
#include <net/entity_id.hpp>
#include <net/input_state.hpp>
#include <serial/writer.hpp>

#include "entity_traits.hpp"
#include "entity_type.hpp"

namespace fpsparty::game {

struct Humanoid {
  void integrate(float dt) noexcept;

  math::box3 bounds() const noexcept;

  math::vec3 local_movement_direction() const noexcept;

  math::vec3 world_movement_direction() const noexcept;

  net::Input_state prev_input_state{};
  net::Input_state curr_input_state{};
  math::vec3 position{math::vec3::Zero()};
  math::vec3 velocity{math::vec3::Zero()};
  float attack_timer{};
  bool grounded{};

  static auto constexpr half_width = 0.35f;
  static auto constexpr height = 1.8f;
  static auto constexpr eye_height = 1.68f;
  static auto constexpr ground_acceleration = 80.0f;
  static auto constexpr air_acceleration = 2.0f;
  // static auto constexpr air_acceleration_speed_limit = 1.0f;
  static auto constexpr walk_speed = 2.5f;
  static auto constexpr run_speed = 5.0f;
  static auto constexpr jump_speed = 7.0f;
  static auto constexpr attack_cooldown = 0.8f;
};

template <> struct Entity_traits<Humanoid> {
  static constexpr Entity_type type = Entity_type::humanoid;

  static void dump(serial::Writer &writer, Humanoid const &entity);
};

} // namespace fpsparty::game

#endif
