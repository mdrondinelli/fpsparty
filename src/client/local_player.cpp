#include "local_player.hpp"

namespace fpsparty::client {

Local_player::Local_player(net::Player_join_request_id request_id) noexcept
    : request_id{request_id} {}

} // namespace fpsparty::client
