#include "enet.hpp"
#include "net/client.hpp"
#include "net/server.hpp"
#include "serial/span_reader.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace {
using namespace fpsparty;

class Test_server : public net::Server {
public:
  explicit Test_server(std::uint16_t port)
      : net::Server{{
          .port = port,
          .max_clients = 1,
        }} {}

  void send_snapshot(
    net::Sequence_number tick_number,
    std::span<std::byte const> grid_state,
    std::span<std::byte const> public_entity_state,
    std::span<std::byte const> player_entity_state) {
    REQUIRE_FALSE(join_requests.empty());
    send_world_snapshot(
      join_requests.front(),
      tick_number,
      grid_state,
      public_entity_state,
      player_entity_state);
  }

  std::vector<enet::Peer> join_requests;

protected:
  void on_peer_connect(enet::Peer) override {}

  void on_peer_disconnect(enet::Peer) override {}

  void on_player_join_request(
    enet::Peer peer, net::Player_join_request_id) override {
    join_requests.push_back(peer);
  }

  void on_player_leave_request(enet::Peer, net::Entity_id) override {}

  void on_player_input_state(
    enet::Peer, net::Entity_id, net::Input_state const &) override {}
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

void poll_until(
  Test_server &server, Snapshot_client &client, auto predicate) {
  for (auto i = 0; i != 10000 && !predicate(); ++i) {
    client.poll_events();
    server.poll_events();
  }
  REQUIRE(predicate());
}
} // namespace

TEST_CASE("World snapshots deliver the server tick number") {
  constexpr auto port = std::uint16_t{29877};
  auto enet_guard = enet::Initialization_guard{};
  auto server = std::unique_ptr<Test_server>{};
  auto client = std::unique_ptr<Snapshot_client>{};
  try {
    enet_guard = enet::Initialization_guard{{}};
    server = std::make_unique<Test_server>(port);
    client = std::make_unique<Snapshot_client>();
  } catch (enet::Initialization_guard::Construction_error const &) {
    SKIP("ENet initialization is unavailable in this environment.");
  } catch (enet::Host::Construction_error const &) {
    SKIP("ENet host creation is unavailable in this environment.");
  }
  client->connect({
    .host = *enet::parse_ip("127.0.0.1"),
    .port = port,
  });
  poll_until(*server, *client, [&] { return client->is_connected(); });
  client->send_player_join_request(1);
  poll_until(*server, *client, [&] { return !server->join_requests.empty(); });

  constexpr auto tick_number = net::Sequence_number{123456};
  server->send_snapshot(tick_number, {}, {}, {});
  poll_until(
    *server, *client, [&] { return client->snapshot_tick_number.has_value(); });
  CHECK(client->snapshot_tick_number == tick_number);
}
