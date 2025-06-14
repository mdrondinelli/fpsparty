#include "server_message_handler.hpp"
#include "message_type.hpp"
#include "serial/serialize.hpp"

namespace fpsparty::net {
void Server_message_handler::on_player_input_state(
    const game::Player::Input_state &) {}

void Server_message_handler::handle_message(serial::Reader &reader) {
  const auto message_type = serial::deserialize<Message_type>(reader);
  if (!message_type) {
    return;
  }
  switch (*message_type) {
  case Message_type::player_input_state: {
    const auto player_input_state =
        serial::deserialize<game::Player::Input_state>(reader);
    if (player_input_state) {
      on_player_input_state(*player_input_state);
    }
    break;
  }
  }
}
} // namespace fpsparty::net
