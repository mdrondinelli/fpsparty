#ifndef FPSPARTY_NET_MESSAGE_TYPE_HPP
#define FPSPARTY_NET_MESSAGE_TYPE_HPP

#include <cstdint>

namespace fpsparty::net {
enum class Message_type : std::uint8_t {
  player_id,
  player_input_state,
  game_state
};
} // namespace fpsparty::net

#endif
