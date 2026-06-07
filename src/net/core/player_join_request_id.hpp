#ifndef FPSPARTY_NET_CORE_PLAYER_JOIN_REQUEST_ID_HPP
#define FPSPARTY_NET_CORE_PLAYER_JOIN_REQUEST_ID_HPP

#include <cstdint>

namespace fpsparty::net {
// 0 is reserved to represent no request.
using Player_join_request_id = std::uint32_t;
} // namespace fpsparty::net

#endif
