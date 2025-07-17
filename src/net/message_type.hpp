#ifndef FPSPARTY_NET_MESSAGE_TYPE_HPP
#define FPSPARTY_NET_MESSAGE_TYPE_HPP

#include <cstdint>

namespace fpsparty::net {
enum class Message_type : std::uint8_t {
  player_join_request,
  player_join_response,
  player_leave_request,
  player_input_state,
  grid_snapshot,
  entity_snapshot
};
} // namespace fpsparty::net

#endif
