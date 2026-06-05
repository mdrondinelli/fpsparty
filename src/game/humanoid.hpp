#ifndef FPSPARTY_GAME_HUMANOID_HPP
#define FPSPARTY_GAME_HUMANOID_HPP

#include <Eigen/Dense>

#include <net/core/entity_id.hpp>
#include <net/core/input_state.hpp>
#include <serial/writer.hpp>

#include "entity_traits.hpp"
#include "entity_type.hpp"

namespace fpsparty::game {

struct Humanoid {
  net::Input_state prev_input_state{};
  net::Input_state curr_input_state{};
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
  float attack_cooldown{};
};

template <>
struct Entity_traits<Humanoid> {
  static constexpr Entity_type type = Entity_type::humanoid;

  static void dump(serial::Writer &writer, Humanoid const &entity);
};

} // namespace fpsparty::game

#endif
