#ifndef FPSPARTY_GAME_HUMANOID_HPP
#define FPSPARTY_GAME_HUMANOID_HPP

#include "game/core/entity.hpp"
#include "game/core/entity_world.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
struct Humanoid_create_info {};

class Humanoid : public Entity {
public:
  explicit Humanoid(net::Entity_id entity_id,
                    const Humanoid_create_info &info) noexcept;

  void on_remove() override;

  const net::Input_state &get_input_state() const noexcept;

  void set_input_state(const net::Input_state &value) noexcept;

  float get_attack_cooldown() const noexcept;

  void set_attack_cooldown(float value) noexcept;

  void decrease_attack_cooldown(float amount) noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &value) noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  void set_velocity(const Eigen::Vector3f &value) noexcept;

private:
  net::Input_state _input_state{};
  float _attack_cooldown{};
  Eigen::Vector3f _position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f _velocity{Eigen::Vector3f::Zero()};
};

class Humanoid_dumper : public Entity_dumper {
public:
  Entity_type get_entity_type() const noexcept override;

  void dump_entity(serial::Writer &writer, const Entity &entity) const override;
};
} // namespace fpsparty::game

#endif
