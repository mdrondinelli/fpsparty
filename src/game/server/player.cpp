#include "player.hpp"
#include "game/core/entity_id.hpp"
#include "game/core/sequence_number.hpp"

namespace fpsparty::game {
Player::Humanoid_remove_listener::Humanoid_remove_listener(
    Player *player) noexcept
    : _player{player} {}

void Player::Humanoid_remove_listener::on_remove_entity() {
  _player->set_humanoid(nullptr);
}

Player::Player(Entity_id entity_id, const Player_create_info &) noexcept
    : Entity{Entity_type::player, entity_id}, _humanoid_remove_listener{this} {}

void Player::on_remove() { set_humanoid(nullptr); }

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
  if (input_sequence_number > *_input_sequence_number) {
    _input_state = input_state;
    _input_sequence_number = input_sequence_number;
  }
}

Entity_type Player_dumper::get_entity_type() const noexcept {
  return Entity_type::player;
}

void Player_dumper::dump_entity(serial::Writer &writer,
                                const Entity &entity) const {
  using serial::serialize;
  if (const auto player = dynamic_cast<const Player *>(&entity)) {
    const auto humanoid = player->get_humanoid().lock();
    const auto humanoid_entity_id = humanoid ? humanoid->get_entity_id() : 0;
    serialize<Entity_id>(writer, humanoid_entity_id);
    if (humanoid_entity_id) {
      serialize<Humanoid_input_state>(writer, player->get_input_state());
      serialize<std::optional<Sequence_number>>(
          writer, player->get_input_sequence_number());
    }
  }
}
} // namespace fpsparty::game
