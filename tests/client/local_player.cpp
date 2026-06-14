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
