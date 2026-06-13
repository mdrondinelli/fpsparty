#ifndef FPSPARTY_CLIENT_SESSION_HPP
#define FPSPARTY_CLIENT_SESSION_HPP

#include <memory>
#include <vector>

#include <net/client.hpp>
#include <net/player_join_request_id.hpp>
#include <scene/scene.hpp>

#include "local_player.hpp"

namespace fpsparty::client {

struct Session_create_info {
  net::Client *client;
  std::uint32_t max_buffered_ticks;
  float tick_duration;
};

class Session {
public:
  explicit Session(Session_create_info const &info);

  void update(float duration);

  Local_player &join_player();

  void leave_player(Local_player &player);

  void on_player_join_response(
    net::Player_join_request_id request_id, net::Entity_id player_entity_id);

  void on_world_snapshot(
    net::Sequence_number tick_number,
    serial::Span_reader &grid_state_reader,
    serial::Span_reader &public_entity_state_reader,
    serial::Span_reader &player_entity_state_reader);

  scene::Scene const &get_scene() const noexcept;

  scene::Scene &get_scene() noexcept;

private:
  enum class State {
    buffering,
    playing,
  };

  void load_player_state(serial::Reader &reader);

  void load_public_state(serial::Reader &reader, scene::Keyframe &keyframe);

  net::Client *_client;
  State _state{State::buffering};
  scene::Scene _scene;
  std::vector<std::unique_ptr<Local_player>> _local_players{};
  net::Player_join_request_id _next_player_join_request_id{1};
  float _tick_timer{};
};

} // namespace fpsparty::client

#endif
