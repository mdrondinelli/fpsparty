#include "client.hpp"
#include "net/client.hpp"

#include <cassert>
#include <iostream>

namespace fpsparty::game {
Client::Client(const Client_create_info &info)
    : net::Client{{
          .incoming_bandwidth = info.incoming_bandwidth,
          .outgoing_bandwidth = info.outgoing_bandwidth,
      }} {}

void Client::service_game_state(float duration) {
  assert(has_game_state());
  _tick_timer -= duration;
  if (_tick_timer <= 0) {
    _tick_timer += _tick_duration;
    const auto player = get_player();
    if (player) {
      const auto input_state = player->get_input_state();
      net::Client::send_player_input_state(*_player_entity_id,
                                           _input_sequence_number, input_state);
      _in_flight_input_states.emplace_back(input_state, _input_sequence_number);
      player->set_input_sequence_number(_input_sequence_number);
      ++_input_sequence_number;
    }
    _game->tick(_tick_duration);
  }
}

bool Client::has_game_state() const noexcept { return _game.has_value(); }

void Client::poll_events() { net::Client::poll_events(); }

void Client::wait_events(std::uint32_t timeout) {
  net::Client::wait_events(timeout);
}

void Client::connect(const enet::Address &address) {
  net::Client::connect(address);
}

bool Client::is_connecting() const noexcept {
  return net::Client::is_connecting();
}

bool Client::is_connected() const noexcept {
  return net::Client::is_connected();
}

Replicated_game *Client::get_game() noexcept {
  return _game ? &*_game : nullptr;
}

const Replicated_game *Client::get_game() const noexcept {
  return _game ? &*_game : nullptr;
}

Replicated_player *Client::get_player() const noexcept {
  return _player_entity_id
             ? dynamic_cast<Replicated_player *>(
                   _game->get_entities().get_entity_by_id(*_player_entity_id))
             : nullptr;
}

void Client::on_connect() {
  _game = Replicated_game{{}};
  send_player_join_request();
}

void Client::on_disconnect() {
  _tick_timer = 0.0f;
  _in_flight_input_states.clear();
  _input_sequence_number = 0;
  _player_entity_id = std::nullopt;
  _game = std::nullopt;
}

void Client::on_player_join_response(net::Entity_id player_entity_id) {
  _player_entity_id = player_entity_id;
  std::cout << "Got player join response. id = " << player_entity_id << ".\n";
}

void Client::on_grid_snapshot(serial::Reader &reader) {
  std::cout << "Got grid snapshot.\n";
  _game->load_grid({.reader = &reader});
  on_update_grid();
}

void Client::on_update_grid() {}

void Client::on_entity_snapshot(net::Sequence_number tick_number,
                                serial::Reader &public_state_reader,
                                serial::Reader &player_state_reader) {
  _game->load_entities({
      .tick_number = tick_number,
      .public_state_reader = &public_state_reader,
      .player_state_reader = &player_state_reader,
  });
  if (const auto player = get_player()) {
    const auto acknowledged_input_sequence_number =
        player->get_input_sequence_number();
    if (acknowledged_input_sequence_number) {
      while (_in_flight_input_states.size() > 0 &&
             _in_flight_input_states[0].second <=
                 *acknowledged_input_sequence_number) {
        _in_flight_input_states.erase(_in_flight_input_states.begin());
      }
    }
    for (const auto &[input_state, input_sequence_number] :
         _in_flight_input_states) {
      player->set_input_state(input_state);
      player->set_input_sequence_number(input_sequence_number);
      _game->tick(_tick_duration);
    }
  }
}

} // namespace fpsparty::game
