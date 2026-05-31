#include "client.hpp"
#include "net/client.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <optional>
#include <tracy/Tracy.hpp>

namespace fpsparty::game {
Client::Client(Client_create_info const &info)
    : net::Client{{
        .incoming_bandwidth = info.incoming_bandwidth,
        .outgoing_bandwidth = info.outgoing_bandwidth,
      }},
      _tick_duration{info.tick_duration} {}

void Client::service_game_state(float duration) {
  assert(has_game_state());
  _tick_timer -= duration;
  if (_tick_timer <= 0) {
    auto const tracy_frame_name = "Tick";
    FrameMarkStart(tracy_frame_name);
    try {
      _tick_timer += _tick_duration;
      if (auto const player = get_player(); player && player->get_humanoid()) {
        net::Client::send_player_input_state(
          *_player_entity_id, _input_sequence_number, _current_input_state);
        _in_flight_input_states
          .emplace_back(_current_input_state, _input_sequence_number);
        player->set_input_state(_current_input_state, _input_sequence_number);
        ++_input_sequence_number;
      }
      _game->tick(_tick_duration);
    } catch (...) {
      FrameMarkEnd(tracy_frame_name);
      throw;
    }
  }
}

bool Client::has_game_state() const noexcept { return _game.has_value(); }

void Client::poll_events() {
  ZoneScoped;
  net::Client::poll_events();
}

void Client::wait_events(std::uint32_t timeout) {
  net::Client::wait_events(timeout);
}

void Client::connect(enet::Address const &address) {
  net::Client::connect(address);
}

bool Client::is_connecting() const noexcept {
  return net::Client::is_connecting();
}

bool Client::is_connected() const noexcept {
  return net::Client::is_connected();
}

net::Input_state const &Client::get_current_input_state() const noexcept {
  return _current_input_state;
}

void Client::set_current_input_state(net::Input_state const &value) noexcept {
  _current_input_state = value;
}

Replicated_game *Client::get_game() noexcept {
  return _game ? &*_game : nullptr;
}

Replicated_game const *Client::get_game() const noexcept {
  return _game ? &*_game : nullptr;
}

Replicated_player *Client::get_player() const noexcept {
  assert(_game);
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
  _last_grid_state_payload.clear();
  _input_sequence_number = 0;
  _player_entity_id = std::nullopt;
  _game = std::nullopt;
}

void Client::on_player_join_response(net::Entity_id player_entity_id) {
  _player_entity_id = player_entity_id;
  std::cout << "Got player join response. id = " << player_entity_id << ".\n";
}

void Client::on_update_grid() {}

void Client::on_world_snapshot(
  net::Sequence_number tick_number,
  serial::Span_reader &grid_state_reader,
  serial::Span_reader &public_entity_state_reader,
  serial::Span_reader &player_entity_state_reader) {
  ZoneScoped;
  auto const grid_state = grid_state_reader.data();
  auto const grid_state_changed =
    grid_state.size() != _last_grid_state_payload.size() ||
    !std::ranges::equal(grid_state, _last_grid_state_payload);
  if (grid_state_changed) {
    _game->load_grid({.reader = &grid_state_reader});
    _last_grid_state_payload.assign(grid_state.begin(), grid_state.end());
    on_update_grid();
  }
  auto initial_player_position = std::optional<Eigen::Vector3f>{};
  if (auto const player = get_player()) {
    initial_player_position = player->get_humanoid()->get_position();
  }
  _game->load_entities({
    .tick_number = tick_number,
    .public_state_reader = &public_entity_state_reader,
    .player_state_reader = &player_entity_state_reader,
  });
  if (auto const player = get_player()) {
    auto const acknowledged_input_sequence_number =
      player->get_input_sequence_number();
    if (acknowledged_input_sequence_number) {
      while (_in_flight_input_states.size() > 0 &&
             _in_flight_input_states[0].second <=
               *acknowledged_input_sequence_number) {
        _in_flight_input_states.erase(_in_flight_input_states.begin());
      }
    }
    for (auto const &[input_state, input_sequence_number] :
         _in_flight_input_states) {
      player->set_input_state(input_state, input_sequence_number);
      _game->tick(_tick_duration);
    }
  }
  auto final_player_position = std::optional<Eigen::Vector3f>{};
  if (auto const player = get_player()) {
    final_player_position = player->get_humanoid()->get_position();
  }
  if (initial_player_position && final_player_position) {
    auto const prediction_error =
      (*initial_player_position - *final_player_position).norm();
    if (prediction_error > 0.01f) {
      std::cout << "Local player position mispredicted by " << prediction_error
                << " meters\n";
      std::cout << _in_flight_input_states.size()
                << " inputs states in flight.\n";
      std::cout << "Input sequence number: "
                << *get_player()->get_input_sequence_number() << "\n";
    }
  }
}
} // namespace fpsparty::game
