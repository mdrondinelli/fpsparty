#include "client.hpp"

#include <cassert>

#include <tracy/Tracy.hpp>

namespace fpsparty::client {

Client::Client(Client_create_info const &info)
    : net::Client{info.net_info},
      _max_buffered_ticks{info.max_buffered_ticks},
      _tick_duration{info.tick_duration} {}

void Client::update(float duration) {
  assert(duration > 0.0f);
  auto const tracy_frame_name = "Client::update";
  try {
    FrameMarkStart(tracy_frame_name);
    if (is_connected()) {
      assert(_session);
      _session->update(duration);
    }
  } catch (...) {
    FrameMarkEnd(tracy_frame_name);
    throw;
  }
  FrameMarkEnd(tracy_frame_name);
}

void Client::on_connect() {
  assert(!_session);
  _session = Session{{
    .client = this,
    .max_buffered_ticks = _max_buffered_ticks,
    .tick_duration = _tick_duration,
  }};
}

void Client::on_disconnect() {
  assert(_session);
  _session = {};
}

void Client::on_player_join_response(
  net::Player_join_request_id request_id, net::Entity_id player_entity_id) {
  assert(_session);
  _session->on_player_join_response(request_id, player_entity_id);
}

void Client::on_world_snapshot(
  net::Sequence_number tick_number,
  serial::Span_reader &grid_state_reader,
  serial::Span_reader &public_entity_state_reader,
  serial::Span_reader &player_entity_state_reader) {
  assert(_session);
  _session->on_world_snapshot(
    tick_number,
    grid_state_reader,
    public_entity_state_reader,
    player_entity_state_reader);
}

} // namespace fpsparty::client
