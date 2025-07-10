#include "player.hpp"

namespace fpsparty::game {
Player::Player(std::uint32_t network_id, const Player_create_info &) noexcept
    : _network_id{network_id}, _humanoid_remove_listener{this} {}

void Player::on_remove() { set_humanoid(nullptr); }

void Player::dump(serial::Writer &writer) const {
  using serial::serialize;
  serialize<std::uint32_t>(writer, _network_id);
  const auto humanoid = _humanoid.lock();
  const auto humanoid_network_id = humanoid ? humanoid->get_network_id() : 0;
  serialize<std::uint32_t>(writer, humanoid_network_id);
  if (humanoid_network_id) {
    serialize<Humanoid_input_state>(writer, _input_state);
    serialize<std::optional<std::uint16_t>>(writer, _input_sequence_number);
  }
}

std::uint32_t Player::get_network_id() const noexcept { return _network_id; }

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

std::optional<std::uint16_t>
Player::get_input_sequence_number() const noexcept {
  return _input_sequence_number;
}

void Player::set_input_state(const Humanoid_input_state &input_state,
                             std::uint16_t input_sequence_number) noexcept {
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
