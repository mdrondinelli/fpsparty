#include "player.hpp"
#include "game/core/game_object_id.hpp"
#include "game/core/sequence_number.hpp"

namespace fpsparty::game {
Player::Player(Game_object_id game_object_id,
               const Player_create_info &) noexcept
    : Game_object{game_object_id}, _humanoid_remove_listener{this} {}

void Player::on_remove() { set_humanoid(nullptr); }

void Player::dump(serial::Writer &writer) const {
  using serial::serialize;
  serialize<Game_object_id>(writer, get_game_object_id());
  const auto humanoid = _humanoid.lock();
  const auto humanoid_game_object_id =
      humanoid ? humanoid->get_game_object_id() : 0;
  serialize<Game_object_id>(writer, humanoid_game_object_id);
  if (humanoid_game_object_id) {
    serialize<Humanoid_input_state>(writer, _input_state);
    serialize<std::optional<Sequence_number>>(writer, _input_sequence_number);
  }
}

const rc::Weak<Humanoid> &Player::get_humanoid() const noexcept {
  return _humanoid;
}

void Player::set_humanoid(const rc::Weak<Humanoid> &value) noexcept {
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

const Humanoid_input_state &Player::get_input_state() const noexcept {
  return _input_state;
}

std::optional<Sequence_number>
Player::get_input_sequence_number() const noexcept {
  return _input_sequence_number;
}

void Player::set_input_state(const Humanoid_input_state &input_state,
                             Sequence_number input_sequence_number) noexcept {
  if (_input_sequence_number) {
    const auto difference = static_cast<std::int16_t>(input_sequence_number -
                                                      *_input_sequence_number);
    if (difference < 0) {
      return;
    }
  }
  _input_state = input_state;
  _input_sequence_number = input_sequence_number;
}
} // namespace fpsparty::game
