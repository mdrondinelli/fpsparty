#include "replicated_player.hpp"
#include "game/client/replicated_humanoid.hpp"
#include "game/core/entity_world.hpp"
#include <iostream>

namespace fpsparty::game {
Replicated_player::Humanoid_remove_listener::Humanoid_remove_listener(
  Replicated_player *player) noexcept
    : _player{player} {}

void Replicated_player::Humanoid_remove_listener::on_remove_entity() {
  _player->set_humanoid(nullptr);
}

Replicated_player::Replicated_player(net::Entity_id entity_id)
    : Entity{Entity_type::player, entity_id}, _humanoid_remove_listener{this} {}

void Replicated_player::on_remove() {
  if (_humanoid) {
    _humanoid->remove_remove_listener(&_humanoid_remove_listener);
  }
}

Replicated_humanoid *Replicated_player::get_humanoid() const noexcept {
  return _humanoid;
}

void Replicated_player::set_humanoid(Replicated_humanoid *value) {
  if (_humanoid) {
    _humanoid->remove_remove_listener(&_humanoid_remove_listener);
  }
  _humanoid = value;
  _input_state = {};
  _input_sequence_number = std::nullopt;
  if (_humanoid) {
    _humanoid->add_remove_listener(&_humanoid_remove_listener);
  }
}

net::Input_state const &Replicated_player::get_input_state() const noexcept {
  return _input_state;
}

void Replicated_player::set_input_state(
  net::Input_state const &value) noexcept {
  _input_state = value;
}

std::optional<net::Sequence_number> const &
Replicated_player::get_input_sequence_number() const noexcept {
  return _input_sequence_number;
}

void Replicated_player::set_input_sequence_number(
  std::optional<net::Sequence_number> const &value) noexcept {
  _input_sequence_number = value;
}

Replicated_player_loader::Replicated_player_loader(
  std::pmr::memory_resource *memory_resource) noexcept
    : _factory{memory_resource} {}

Entity_owner<Entity>
Replicated_player_loader::create_entity(net::Entity_id entity_id) {
  return _factory.create(entity_id);
}

void Replicated_player_loader::load_entity(
  serial::Reader &reader, Entity &entity, Entity_world const &world) const {
  auto const player = dynamic_cast<Replicated_player *>(&entity);
  if (!player) {
    std::cerr << "Failed to downcast player.\n";
    throw Replicated_player_load_error{};
  }
  auto const humanoid_entity_id = deserialize<net::Entity_id>(reader);
  if (!humanoid_entity_id) {
    std::cerr << "Failed to deserialize humanoid entity id.\n";
    throw Replicated_player_load_error{};
  }
  if (*humanoid_entity_id) {
    auto const humanoid = dynamic_cast<Replicated_humanoid *>(
      world.get_entity_by_id(*humanoid_entity_id));
    if (!humanoid) {
      std::cerr << "Failed to get humanoid by entity id.\n";
      throw Replicated_player_load_error{};
    }
    auto const input_state = deserialize<net::Input_state>(reader);
    if (!input_state) {
      std::cerr << "Failed to get deserialize input state.\n";
      throw Replicated_player_load_error{};
    }
    auto const input_sequence_number =
      deserialize<std::optional<net::Sequence_number>>(reader);
    if (!input_sequence_number) {
      std::cerr << "Failed to get deserialize input sequence number.\n";
      throw Replicated_player_load_error{};
    }
    player->set_humanoid(humanoid);
    player->set_input_state(*input_state);
    player->set_input_sequence_number(*input_sequence_number);
  }
}

Entity_type Replicated_player_loader::get_entity_type() const noexcept {
  return Entity_type::player;
}
} // namespace fpsparty::game
