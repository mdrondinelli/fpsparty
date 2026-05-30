#include "constants.hpp"
#include "enet.hpp"
#include "game/server/server.hpp"
#include "net/core/constants.hpp"
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>

using namespace fpsparty;

namespace {
std::sig_atomic_t volatile signal_status{};
void handle_signal(int signal) { signal_status = signal; }

void fill_blocks(
  game::Server &server,
  Eigen::Vector3i const &min,
  Eigen::Vector3i const &max,
  bool solid = true) {
  server.get_game().get_grid().fill({min, max}, solid);
}
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  auto const enet_guard = enet::Initialization_guard{{}};
  auto server = game::Server{{
    .net_info =
      {
        .port = net::constants::port,
        .max_clients = net::constants::max_clients,
      },
    .game_info = {.grid_info = {.width = 32, .height = 32, .depth = 32}},
    .tick_duration = constants::tick_duration,
  }};
  // green floor
  fill_blocks(server, {0, 0, 0}, {16, 1, 16});
  std::cout << "Server running on port " << net::constants::port << ".\n";
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;
  constexpr auto blink_block_period = 0.5f;
  auto blink_block_timer = 0.0f;
  auto blink_block_solid = false;
  auto loop_duration = Duration{};
  auto loop_time = Clock::now();
  while (!signal_status) {
    server.poll_events();
    auto const duration =
      std::chrono::duration_cast<std::chrono::duration<float>>(loop_duration)
        .count();
    blink_block_timer -= duration;
    if (blink_block_timer <= 0.0f) {
      blink_block_timer += blink_block_period;
      blink_block_solid = !blink_block_solid;
      fill_blocks(server, {8, 3, 8}, {9, 4, 9}, blink_block_solid);
    }
    if (server.service_game_state(duration)) {
      server.broadcast_game_state();
    }
    auto const now = Clock::now();
    loop_duration = now - loop_time;
    loop_time = now;
  }
  if (signal_status == SIGINT) {
    server.disconnect();
    do {
      server.wait_events(100);
    } while (server.get_peer_count() > 0);
  }
  std::cout << "Exiting.\n";
  return 0;
}
