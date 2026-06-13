#include "client/client.hpp"
#include "enet.hpp"
#include "game/entity_type.hpp"
#include "game/grid.hpp"
#include "net/constants.hpp"
#include "net/server.hpp"
#include "serial/ostream_writer.hpp"
#include "serial/serialize.hpp"
#include "serial/span_reader.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace fpsparty;

class Test_server : public net::Server {
public:
  struct Join_request {
    enet::Peer peer;
    net::Player_join_request_id request_id;
  };

  struct Input {
    net::Entity_id player_entity_id;
    net::Input_state state;
  };

  explicit Test_server(std::uint16_t port)
      : net::Server{{
          .port = port,
          .max_clients = 1,
        }} {}

  void respond_to_join(std::size_t index, net::Entity_id player_entity_id) {
    auto const request = join_requests.at(index);
    send_player_join_response(
      request.peer, request.request_id, player_entity_id);
  }

  void send_snapshot(
    net::Sequence_number tick_number,
    std::span<std::byte const> grid_state,
    std::span<std::byte const> public_entity_state,
    std::span<std::byte const> player_entity_state) {
    REQUIRE_FALSE(join_requests.empty());
    send_world_snapshot(
      join_requests.front().peer,
      tick_number,
      grid_state,
      public_entity_state,
      player_entity_state);
  }

  std::vector<Join_request> join_requests;
  std::vector<net::Entity_id> leave_requests;
  std::vector<Input> inputs;

protected:
  void on_peer_connect(enet::Peer) override {}

  void on_peer_disconnect(enet::Peer) override {}

  void on_player_join_request(
    enet::Peer peer, net::Player_join_request_id request_id) override {
    join_requests.push_back({peer, request_id});
  }

  void on_player_leave_request(
    enet::Peer, net::Entity_id player_entity_id) override {
    leave_requests.push_back(player_entity_id);
  }

  void on_player_input_state(
    enet::Peer,
    net::Entity_id player_entity_id,
    net::Input_state const &input_state) override {
    inputs.push_back({player_entity_id, input_state});
  }
};

class Snapshot_client : public net::Client {
public:
  Snapshot_client() : net::Client{{}} {}

  std::optional<net::Sequence_number> snapshot_tick_number;

protected:
  void on_connect() override {}

  void on_disconnect() override {}

  void on_player_join_response(
    net::Player_join_request_id, net::Entity_id) override {}

  void on_world_snapshot(
    net::Sequence_number tick_number,
    serial::Span_reader &,
    serial::Span_reader &,
    serial::Span_reader &) override {
    snapshot_tick_number = tick_number;
  }
};

template <typename Client, typename Predicate>
void poll_until(Test_server &server, Client &client, Predicate predicate) {
  for (auto i = 0; i != 10000 && !predicate(); ++i) {
    client.poll_events();
    server.poll_events();
  }
  REQUIRE(predicate());
}

std::vector<std::byte> bytes(serial::Ostringstream_writer const &writer) {
  auto const view = writer.stream().view();
  auto result = std::vector<std::byte>(view.size());
  std::memcpy(result.data(), view.data(), view.size());
  return result;
}
} // namespace

TEST_CASE("Player join request IDs serialize in network messages") {
  auto writer = fpsparty::serial::Ostringstream_writer{};
  auto const request_id = fpsparty::net::Player_join_request_id{12345};
  fpsparty::serial::serialize<fpsparty::net::Player_join_request_id>(
    writer, request_id);
  auto const data = bytes(writer);
  auto reader = fpsparty::serial::Span_reader{data};
  CHECK(
    fpsparty::serial::deserialize<fpsparty::net::Player_join_request_id>(
      reader) == request_id);
}

TEST_CASE("World snapshots deliver the server tick number") {
  constexpr auto port = std::uint16_t{29877};
  auto enet_guard = fpsparty::enet::Initialization_guard{};
  auto server = std::unique_ptr<Test_server>{};
  auto client = std::unique_ptr<Snapshot_client>{};
  try {
    enet_guard = fpsparty::enet::Initialization_guard{{}};
    server = std::make_unique<Test_server>(port);
    client = std::make_unique<Snapshot_client>();
  } catch (fpsparty::enet::Initialization_guard::Construction_error const &) {
    SKIP("ENet initialization is unavailable in this environment.");
  } catch (fpsparty::enet::Host::Construction_error const &) {
    SKIP("ENet host creation is unavailable in this environment.");
  }
  client->connect({
    .host = *fpsparty::enet::parse_ip("127.0.0.1"),
    .port = port,
  });
  poll_until(*server, *client, [&] { return client->is_connected(); });
  client->send_player_join_request(1);
  poll_until(*server, *client, [&] { return !server->join_requests.empty(); });

  constexpr auto tick_number = fpsparty::net::Sequence_number{123456};
  server->send_snapshot(tick_number, {}, {}, {});
  poll_until(
    *server, *client, [&] { return client->snapshot_tick_number.has_value(); });
  CHECK(client->snapshot_tick_number == tick_number);
}

/*
TEST_CASE("Client correlates multiple local player joins") {
  constexpr auto port = std::uint16_t{29876};
  auto enet_guard = fpsparty::enet::Initialization_guard{};
  auto server = std::unique_ptr<Test_server>{};
  auto client = std::unique_ptr<fpsparty::client::Client>{};
  try {
    enet_guard = fpsparty::enet::Initialization_guard{{}};
    server = std::make_unique<Test_server>(port);
    client = std::make_unique<fpsparty::client::Client>(
      fpsparty::client::Client_create_info{
        .net_info = {},
        .tick_duration = 1.0f / 60.0f,
      });
  } catch (fpsparty::enet::Initialization_guard::Construction_error const &) {
    SKIP("ENet initialization is unavailable in this environment.");
  } catch (fpsparty::enet::Host::Construction_error const &) {
    SKIP("ENet host creation is unavailable in this environment.");
  }
  client->connect({
    .host = *fpsparty::enet::parse_ip("127.0.0.1"),
    .port = port,
  });
  poll_until(*server, *client, [&] { return client->is_connected(); });

  auto &first = client->join_player();
  auto &second = client->join_player();
  auto const first_address = &first;
  auto const second_address = &second;
  poll_until(
    *server, *client, [&] { return server->join_requests.size() == 2; });
  CHECK(server->join_requests[0].request_id != 0);
  CHECK(
    server->join_requests[1].request_id ==
    server->join_requests[0].request_id + 1);

  server->respond_to_join(1, 22);
  server->respond_to_join(0, 11);
  poll_until(*server, *client, [&] {
    return first.get_state() == fpsparty::client::Local_player_state::joined &&
           second.get_state() == fpsparty::client::Local_player_state::joined;
  });
  CHECK(first.get_entity_id() == 11);
  CHECK(second.get_entity_id() == 22);

  auto &third = client->join_player();
  CHECK(&first == first_address);
  CHECK(&second == second_address);
  CHECK(third.get_state() == fpsparty::client::Local_player_state::joining);

  auto first_input = fpsparty::net::Input_state{.move_left = true};
  auto second_input = fpsparty::net::Input_state{.move_right = true};
  first.set_input_state(first_input);
  second.set_input_state(second_input);
  client->tick(1.0f);
  poll_until(*server, *client, [&] { return server->inputs.size() == 2; });
  CHECK(server->inputs[0].player_entity_id == 11);
  CHECK(server->inputs[0].state.move_left);
  CHECK(server->inputs[1].player_entity_id == 22);
  CHECK(server->inputs[1].state.move_right);

  auto grid_writer = fpsparty::serial::Ostringstream_writer{};
  fpsparty::game::Grid{{}}.dump(grid_writer);
  auto player_writer = fpsparty::serial::Ostringstream_writer{};
  fpsparty::serial::serialize<std::uint32_t>(player_writer, std::uint32_t{2});
  for (auto const [player_entity_id, humanoid_entity_id] :
       {std::pair{11u, 101u}, std::pair{22u, 102u}}) {
    fpsparty::serial::serialize<fpsparty::game::Entity_type>(
      player_writer, fpsparty::game::Entity_type::player);
    fpsparty::serial::serialize<fpsparty::net::Entity_id>(
      player_writer, player_entity_id);
    fpsparty::serial::serialize<fpsparty::net::Entity_id>(
      player_writer, humanoid_entity_id);
  }
  auto public_writer = fpsparty::serial::Ostringstream_writer{};
  fpsparty::serial::serialize<std::uint32_t>(public_writer, std::uint32_t{3});
  for (auto const [entity_id, x] :
       {std::pair{101u, 1.0f}, std::pair{102u, 2.0f}, std::pair{999u, 3.0f}}) {
    fpsparty::serial::serialize<fpsparty::game::Entity_type>(
      public_writer, fpsparty::game::Entity_type::humanoid);
    fpsparty::serial::serialize<fpsparty::net::Entity_id>(
      public_writer, entity_id);
    auto const position = Eigen::Vector3f{x, 0.0f, 0.0f};
    fpsparty::serial::serialize<Eigen::Vector3f>(public_writer, position);
    auto const input_state = fpsparty::net::Input_state{};
    fpsparty::serial::serialize<fpsparty::net::Input_state>(
      public_writer, input_state);
  }
  auto const grid_state = bytes(grid_writer);
  auto const public_state = bytes(public_writer);
  auto const player_state = bytes(player_writer);
  server->send_snapshot(7, grid_state, public_state, player_state);
  poll_until(*server, *client, [&] {
    return first.get_camera_snapshot() && second.get_camera_snapshot();
  });
  CHECK(first.get_camera_snapshot()->position.x() == 1.0f);
  CHECK(second.get_camera_snapshot()->position.x() == 2.0f);
  REQUIRE(client->get_scene());
  CHECK(client->get_scene()->get_mesh_instances().size() == 1);

  client->leave_player(first);
  poll_until(
    *server, *client, [&] { return server->leave_requests.size() == 1; });
  CHECK(server->leave_requests.front() == 11);
  CHECK(&second == second_address);
}
*/
