#include "replicated_humanoid.hpp"
#include "game/core/game_object_id.hpp"

namespace fpsparty::game {
Replicated_humanoid::Replicated_humanoid(Game_object_id game_object_id) noexcept
    : Game_object{game_object_id} {}

void Replicated_humanoid::on_remove() {}

const Humanoid_input_state &
Replicated_humanoid::get_input_state() const noexcept {
  return _input_state;
}

void Replicated_humanoid::set_input_state(
    const Humanoid_input_state &value) noexcept {
  _input_state = value;
}

const Eigen::Vector3f &Replicated_humanoid::get_position() const noexcept {
  return _position;
}

void Replicated_humanoid::set_position(const Eigen::Vector3f &value) noexcept {
  _position = value;
}
} // namespace fpsparty::game
