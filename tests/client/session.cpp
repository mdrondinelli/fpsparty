// Tests for the latency-based buffering state machine in client::Session.
//
// API contract under test:
//   enum class Session_state { buffering, playing };   // fpsparty::client, namespace scope
//   struct Session_create_info {
//     net::Client_outbox *client;   // outbound-send seam (no socket in tests)
//     float tick_duration;
//     float min_latency;            // resume playing at this lead
//     float max_latency;            // forward-jump above this lead
//   };
//   Session_state Session::get_state() const noexcept;
//
// The buffering policy lives in Session::update and reads only the scene clock,
// so tests feed keyframes straight into get_scene() and drive update() -- no
// socket, no serialized snapshots.

#include "client/session.hpp"

#include "game/grid.hpp"
#include "net/client.hpp"
#include "scene/scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace {
using namespace fpsparty;
using Catch::Approx;

constexpr float kd = 0.1f;

// Outbound seam stub: the buffering tests never join a player, so the sends are
// never invoked; these exist only so Session has a non-socket outbox.
struct Recording_outbox : net::Client_outbox {
  void send_player_join_request(net::Player_join_request_id) override {}
  void send_player_leave_request(net::Entity_id) override {}
  void send_player_input_state(
    net::Entity_id, net::Input_state const &) override {}
};

scene::Keyframe make_keyframe(std::uint64_t number) {
  return scene::Keyframe{
    .number = number,
    .grid = game::Grid{{}},
    .cameras = {},
    .mesh_instances = {},
  };
}

client::Session
make_session(net::Client_outbox &outbox, float min_latency, float max_latency) {
  return client::Session{{
    .client = &outbox,
    .tick_duration = kd,
    .min_latency = min_latency,
    .max_latency = max_latency,
  }};
}

void buffer(client::Session &session, std::uint64_t from, std::uint64_t to) {
  for (auto n = from; n <= to; ++n) {
    session.get_scene().push(make_keyframe(n));
  }
}
} // namespace

TEST_CASE("Session starts in the buffering state") {
  auto outbox = Recording_outbox{};
  auto session = make_session(outbox, 0.2f, 1.0f);
  CHECK(session.get_state() == client::Session_state::buffering);
}

TEST_CASE("Session keeps buffering below min_latency") {
  auto outbox = Recording_outbox{};
  auto session = make_session(outbox, 0.2f, 1.0f);
  buffer(session, 1, 2); // lead = 1 tick = 0.1s < min_latency
  session.update(kd);
  CHECK(session.get_state() == client::Session_state::buffering);
}

TEST_CASE("Session begins playing once latency reaches min_latency") {
  auto outbox = Recording_outbox{};
  auto session = make_session(outbox, 0.2f, 1.0f);
  buffer(session, 1, 3); // lead = 2 ticks = min_latency
  session.update(kd);
  CHECK(session.get_state() == client::Session_state::playing);
}

TEST_CASE("Session re-enters buffering when playback starves") {
  auto outbox = Recording_outbox{};
  auto session = make_session(outbox, 0.1f, 1.0f);
  buffer(session, 1, 2); // lead = 1 tick = min_latency
  session.update(kd);    // buffering -> playing, then advances onto newest
  REQUIRE(session.get_state() == client::Session_state::playing);
  session.update(kd); // stuck at newest, no progress -> starve -> buffering
  CHECK(session.get_state() == client::Session_state::buffering);
}

TEST_CASE("Session jumps forward when latency exceeds max_latency") {
  auto outbox = Recording_outbox{};
  auto session = make_session(outbox, 0.1f, 0.3f);
  buffer(session, 1, 11); // lead = 10 ticks = 1.0s > max_latency
  session.update(0.01f);  // buffering -> playing, then jump lead down to min
  CHECK(session.get_state() == client::Session_state::playing);
  CHECK(session.get_scene().get_latency() == Approx(0.1f));
}
