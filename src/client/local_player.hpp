#ifndef FPSPARTY_CLIENT_LOCAL_PLAYER_HPP
#define FPSPARTY_CLIENT_LOCAL_PLAYER_HPP

#include <optional>

#include <Eigen/Dense>

#include <net/entity_id.hpp>
#include <net/input_state.hpp>
#include <net/player_join_request_id.hpp>

namespace fpsparty::client {

class Client;

enum class Local_player_state {
  joining,
  joined,
};

struct Local_player {
  explicit Local_player(net::Player_join_request_id request_id) noexcept;

  net::Player_join_request_id request_id;
  Local_player_state state{Local_player_state::joining};
  net::Input_state input_state{};
  std::optional<net::Entity_id> player_entity_id{};
  std::optional<net::Entity_id> humanoid_entity_id{};
};

} // namespace fpsparty::client

#endif
