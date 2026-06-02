#ifndef FPSPARTY_GAME_HUMANOID_HPP
#define FPSPARTY_GAME_HUMANOID_HPP

#include "game/entity.hpp"
#include "game/entity_world.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
struct Humanoid_create_info {};

class Humanoid : public Entity {
public:
  explicit Humanoid(
    net::Entity_id entity_id, Humanoid_create_info const &info) noexcept;

  void on_remove() override;

  net::Input_state const &get_input_state() const noexcept;

  net::Input_state const &get_prev_input_state() const noexcept;

  void set_input_state(net::Input_state const &value) noexcept;

  float get_attack_cooldown() const noexcept;

  void set_attack_cooldown(float value) noexcept;

  void decrease_attack_cooldown(float amount) noexcept;

  Eigen::Vector3f const &get_position() const noexcept;

  void set_position(Eigen::Vector3f const &value) noexcept;

  Eigen::Vector3f const &get_velocity() const noexcept;

  void set_velocity(Eigen::Vector3f const &value) noexcept;

private:
  net::Input_state _input_state{};
  net::Input_state _prev_input_state{};
  float _attack_cooldown{};
  Eigen::Vector3f _position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f _velocity{Eigen::Vector3f::Zero()};
};

class Humanoid_dumper : public Entity_dumper {
public:
  Entity_type get_entity_type() const noexcept override;

  void dump_entity(serial::Writer &writer, Entity const &entity) const override;
};
} // namespace fpsparty::game

#endif
