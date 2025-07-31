#include "client.hpp"
#include "net/client.hpp"

#include <cassert>
#include <iostream>

namespace fpsparty::game {
Client::Net_client::Net_client(const net::Client_create_info &info,
                               game::Client *game_client) noexcept
    : net::Client{info}, _game_client{game_client} {}

Client::Client(const Client_create_info &info)
    : _net_client{{
                      .incoming_bandwidth = info.incoming_bandwidth,
                      .outgoing_bandwidth = info.outgoing_bandwidth,
                  },
                  this} {}

void Client::Net_client::on_connect() {}

void Client::Net_client::on_disconnect() {
  _game_client->_tick_timer = 0.0f;
  _game_client->_in_flight_input_states.clear();
  _game_client->_input_sequence_number = 0;
  _game_client->_player_entity_id = std::nullopt;
  _game_client->_game = std::nullopt;
}

void Client::Net_client::on_player_join_response(
    net::Entity_id player_entity_id) {
  _game_client->_player_entity_id = player_entity_id;
  std::cout << "Got player join response. id = " << player_entity_id << ".\n";
}

void Client::Net_client::on_grid_snapshot(serial::Reader &) {}

void Client::Net_client::on_entity_snapshot(
    net::Sequence_number tick_number, serial::Reader &public_state_reader,
    serial::Reader &player_state_reader) {
  if (!_game_client->_game) {
    send_player_join_request();
    _game_client->_game = Replicated_game{{}};
  }
  _game_client->_game->load({
      .tick_number = tick_number,
      .public_state_reader = &public_state_reader,
      .player_state_reader = &player_state_reader,
  });
  if (const auto player = _game_client->get_player()) {
    const auto acknowledged_input_sequence_number =
        player->get_input_sequence_number();
    if (acknowledged_input_sequence_number) {
      while (_game_client->_in_flight_input_states.size() > 0 &&
             _game_client->_in_flight_input_states[0].second <=
                 *acknowledged_input_sequence_number) {
        _game_client->_in_flight_input_states.erase(
            _game_client->_in_flight_input_states.begin());
      }
    }
    for (const auto &[input_state, input_sequence_number] :
         _game_client->_in_flight_input_states) {
      player->set_input_state(input_state);
      player->set_input_sequence_number(input_sequence_number);
      _game_client->_game->tick(_game_client->_tick_duration);
    }
  }
}

void Client::service_game_state(float duration) {
  assert(has_game_state());
  _tick_timer -= duration;
  if (_tick_timer <= 0) {
    _tick_timer += _tick_duration;
    const auto player = get_player();
    if (player) {
      const auto input_state = player->get_input_state();
      _net_client.send_player_input_state(*_player_entity_id,
                                          _input_sequence_number, input_state);
      _in_flight_input_states.emplace_back(input_state, _input_sequence_number);
      player->set_input_sequence_number(_input_sequence_number);
      ++_input_sequence_number;
    }
    _game->tick(_tick_duration);
  }
}

bool Client::has_game_state() const noexcept { return _game.has_value(); }

void Client::poll_events() { _net_client.poll_events(); }

void Client::wait_events(std::uint32_t timeout) {
  _net_client.wait_events(timeout);
}

void Client::connect(const enet::Address &address) {
  _net_client.connect(address);
}

bool Client::is_connecting() const noexcept {
  return _net_client.is_connecting();
}

bool Client::is_connected() const noexcept {
  return _net_client.is_connected();
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
                   _game->get_world().get_entity_by_id(*_player_entity_id))
             : nullptr;
}
} // namespace fpsparty::game
