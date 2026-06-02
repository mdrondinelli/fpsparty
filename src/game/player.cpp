#include "player.hpp"
#include "net/core/entity_id.hpp"

namespace fpsparty::game {
Player::Humanoid_remove_listener::Humanoid_remove_listener(
  Player *player) noexcept
    : _player{player} {}

void Player::Humanoid_remove_listener::on_remove_entity() {
  _player->set_humanoid(nullptr);
}

Player::Player(net::Entity_id entity_id, Player_create_info const &) noexcept
    : Entity{Entity_type::player, entity_id}, _humanoid_remove_listener{this} {}

void Player::on_remove() { set_humanoid(nullptr); }

Humanoid *Player::get_humanoid() const noexcept { return _humanoid; }

void Player::set_humanoid(Humanoid *value) noexcept {
  if (_humanoid) {
    _humanoid->remove_remove_listener(&_humanoid_remove_listener);
  }
  _humanoid = value;
  _input_state = {};
  if (_humanoid) {
    _humanoid->add_remove_listener(&_humanoid_remove_listener);
  }
}

net::Input_state const &Player::get_input_state() const noexcept {
  return _input_state;
}

void Player::set_input_state(net::Input_state const &input_state) noexcept {
  _input_state = input_state;
}

Entity_type Player_dumper::get_entity_type() const noexcept {
  return Entity_type::player;
}

void Player_dumper::dump_entity(
  serial::Writer &writer, Entity const &entity) const {
  using serial::serialize;
  if (auto const player = dynamic_cast<Player const *>(&entity)) {
    auto const humanoid = player->get_humanoid();
    auto const humanoid_entity_id = humanoid ? humanoid->get_entity_id() : 0;
    serialize<net::Entity_id>(writer, humanoid_entity_id);
  }
}
} // namespace fpsparty::game
