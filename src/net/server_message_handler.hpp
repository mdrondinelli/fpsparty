#ifndef FPSPARTY_NET_SERVER_MESSAGE_HANDLER_HPP
#define FPSPARTY_NET_SERVER_MESSAGE_HANDLER_HPP

#include "game/game.hpp"
#include "serial/reader.hpp"

namespace fpsparty::net {
class Server_message_handler {
public:
  virtual ~Server_message_handler() = default;

  virtual void
  on_player_input_state(const game::Player::Input_state &player_input_state);

  void handle_message(serial::Reader &reader);
};
} // namespace fpsparty::net

#endif
