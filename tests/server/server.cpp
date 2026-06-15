#include "server/server.hpp"
#include "enet.hpp"
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace {
using namespace fpsparty;

std::unique_ptr<server::Server> make_server() {
  return std::make_unique<server::Server>(server::Server_create_info{
    .net_info =
      {
        .port = 0,
        .max_clients = 1,
      },
    .game_info =
      {.grid_info =
         {
           .bounds =
             math::ibox3{
               math::ivec3{0, 0, 0},
               math::ivec3{3, 3, 3},
             },
         }},
    .tick_duration = 1.0f,
  });
}
} // namespace

TEST_CASE("Server tick numbers track completed simulation ticks") {
  auto enet_guard = fpsparty::enet::Initialization_guard{};
  auto server = std::unique_ptr<fpsparty::server::Server>{};
  try {
    enet_guard = fpsparty::enet::Initialization_guard{{}};
    server = make_server();
  } catch (fpsparty::enet::Initialization_guard::Construction_error const &) {
    SKIP("ENet initialization is unavailable in this environment.");
  } catch (fpsparty::enet::Host::Construction_error const &) {
    SKIP("ENet host creation is unavailable in this environment.");
  }

  CHECK(server->get_tick_number() == 0);

  CHECK(server->tick(0.0f));
  CHECK(server->get_tick_number() == 1);

  CHECK_FALSE(server->tick(0.5f));
  CHECK(server->get_tick_number() == 1);

  server->broadcast_game_state();
  CHECK(server->get_tick_number() == 1);

  CHECK(server->tick(0.5f));
  CHECK(server->get_tick_number() == 2);
}
