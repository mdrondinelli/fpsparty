#include "replicated_player.hpp"

namespace fpsparty::game {
Replicated_player::Humanoid_remove_listener::Humanoid_remove_listener(
    Replicated_player *player) noexcept
    : _player{player} {}

void Replicated_player::Humanoid_remove_listener::on_remove_game_object() {
  _player->set_humanoid(nullptr);
}

Replicated_player::Replicated_player(Game_object_id game_object_id)
    : Game_object{game_object_id}, _humanoid_remove_listener{this} {}

void Replicated_player::on_remove() {
  const auto humanoid = _humanoid.lock();
  if (humanoid) {
    humanoid->remove_remove_listener(&_humanoid_remove_listener);
  }
}

const rc::Weak<Replicated_humanoid> &
Replicated_player::get_humanoid() const noexcept {
  return _humanoid;
}

void Replicated_player::set_humanoid(
    const rc::Weak<Replicated_humanoid> &value) noexcept {
  if (const auto humanoid = _humanoid.lock()) {
    humanoid->remove_remove_listener(&_humanoid_remove_listener);
  }
  _humanoid = value;
  _input_state = {};
  _input_sequence_number = std::nullopt;
  if (const auto humanoid = _humanoid.lock()) {
    humanoid->add_remove_listener(&_humanoid_remove_listener);
  }
}

const Humanoid_input_state &
Replicated_player::get_input_state() const noexcept {
  return _input_state;
}

void Replicated_player::set_input_state(
    const Humanoid_input_state &value) noexcept {
  _input_state = value;
}

const std::optional<Sequence_number> &
Replicated_player::get_input_sequence_number() const noexcept {
  return _input_sequence_number;
}

void Replicated_player::set_input_sequence_number(
    const std::optional<Sequence_number> &value) noexcept {
  _input_sequence_number = value;
}
} // namespace fpsparty::game
