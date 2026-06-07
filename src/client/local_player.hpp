#ifndef FPSPARTY_CLIENT_LOCAL_PLAYER_HPP
#define FPSPARTY_CLIENT_LOCAL_PLAYER_HPP

#include <optional>

#include <Eigen/Dense>

#include <net/core/entity_id.hpp>
#include <net/core/input_state.hpp>
#include <net/core/player_join_request_id.hpp>

namespace fpsparty::client {

class Client;

struct Camera_snapshot {
  Eigen::Vector3f position{};
  float yaw{};
  float pitch{};
};

enum class Local_player_state {
  joining,
  joined,
};

class Local_player {
public:
  Local_player_state get_state() const noexcept;

  net::Input_state const &get_input_state() const noexcept;

  void set_input_state(net::Input_state const &value) noexcept;

  std::optional<net::Entity_id> const &get_entity_id() const noexcept;

  std::optional<Camera_snapshot> const &get_camera_snapshot() const noexcept;

private:
  friend Client;

  explicit Local_player(net::Player_join_request_id request_id) noexcept;

  net::Player_join_request_id _request_id{};
  Local_player_state _state{Local_player_state::joining};
  net::Input_state _input_state{};
  std::optional<net::Entity_id> _player_entity_id{};
  std::optional<net::Entity_id> _humanoid_entity_id{};
  std::optional<Camera_snapshot> _camera_snapshot{};
};

} // namespace fpsparty::client

#endif
