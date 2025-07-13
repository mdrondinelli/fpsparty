#ifndef FPSPARTY_GAME_HUMANOID_HPP
#define FPSPARTY_GAME_HUMANOID_HPP

#include "game/core/game_object.hpp"
#include "game/core/game_object_id.hpp"
#include "game/core/humanoid_input_state.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
struct Humanoid_create_info {};

class Humanoid : public Game_object, public rc::Object<Humanoid> {
public:
  explicit Humanoid(Game_object_id game_object_id,
                    const Humanoid_create_info &info) noexcept;

  void on_remove() override;

  const Humanoid_input_state &get_input_state() const noexcept;

  void set_input_state(const Humanoid_input_state &value) noexcept;

  float get_attack_cooldown() const noexcept;

  void set_attack_cooldown(float value) noexcept;

  void decrease_attack_cooldown(float amount) noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &value) noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  void set_velocity(const Eigen::Vector3f &value) noexcept;

private:
  Humanoid_input_state _input_state{};
  float _attack_cooldown{};
  Eigen::Vector3f _position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f _velocity{Eigen::Vector3f::Zero()};
};
} // namespace fpsparty::game

#endif
