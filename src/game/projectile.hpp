#ifndef FPSPARTY_GAME_PROJECTILE_HPP
#define FPSPARTY_GAME_PROJECTILE_HPP

#include <Eigen/Dense>

#include <serial/writer.hpp>

#include "entity_handle.hpp"
#include "entity_traits.hpp"
#include "entity_type.hpp"

namespace fpsparty::game {

struct Humanoid;

struct Projectile {
  Entity_handle<Humanoid> creator{};
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
};

template <>
struct Entity_traits<Projectile> {
  static constexpr Entity_type type = Entity_type::projectile;

  static void dump(serial::Writer &writer, Projectile const &entity);
};

} // namespace fpsparty::game

#endif
