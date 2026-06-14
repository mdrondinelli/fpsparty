#ifndef FPSPARTY_CLIENT_SESSION_HPP
#define FPSPARTY_CLIENT_SESSION_HPP

#include <memory>
#include <vector>

#include <net/client.hpp>
#include <net/player_join_request_id.hpp>
#include <scene/scene.hpp>

#include "local_player.hpp"

namespace fpsparty::client {

enum class Session_state {
  buffering,
  playing,
};

struct Session_create_info {
  net::Client_outbox *client;
  float tick_duration;
  float min_latency;
  float max_latency;
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

  Session_state get_state() const noexcept;

private:
  void load_player_state(serial::Reader &reader);

  void load_public_state(serial::Reader &reader, scene::Keyframe &keyframe);

  net::Client_outbox *_client;
  Session_state _state{Session_state::buffering};
  float _min_latency;
  float _max_latency;
  scene::Scene _scene;
  std::vector<std::unique_ptr<Local_player>> _local_players{};
  net::Player_join_request_id _next_player_join_request_id{1};
  float _tick_timer{};
};

} // namespace fpsparty::client

#endif
