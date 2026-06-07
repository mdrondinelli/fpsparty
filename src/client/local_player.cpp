#include "local_player.hpp"

namespace fpsparty::client {
Local_player::Local_player(net::Player_join_request_id request_id) noexcept
    : _request_id{request_id} {}

Local_player_state Local_player::get_state() const noexcept {
  return _state;
}

std::optional<net::Entity_id> const &Local_player::get_entity_id() const noexcept {
  return _player_entity_id;
}

net::Input_state const &Local_player::get_input_state() const noexcept {
  return _input_state;
}

void Local_player::set_input_state(net::Input_state const &value) noexcept {
  _input_state = value;
}

std::optional<Camera_snapshot> const &
Local_player::get_camera_snapshot() const noexcept {
  return _camera_snapshot;
}
} // namespace fpsparty::client
