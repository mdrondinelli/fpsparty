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
    .game_info = {.grid_info = {.width = 256, .height = 32, .depth = 256}},
    .tick_duration = constants::tick_duration,
  }};
  // green floor
  fill_blocks(server, {0, 0, 0}, {16, 1, 16});
  // bottom rims
  fill_blocks(server, {3, 1, 3}, {13, 2, 13});
  fill_blocks(server, {4, 1, 4}, {12, 2, 12}, false);
  // top rims
  fill_blocks(server, {3, 2, 3}, {13, 3, 13});
  fill_blocks(server, {4, 2, 4}, {12, 3, 12}, false);
  // red walls
  fill_blocks(server, {4, 0, 4}, {5, 1, 12});
  fill_blocks(server, {12, 0, 4}, {13, 1, 12});
  fill_blocks(server, {4, 2, 4}, {5, 3, 12});
  fill_blocks(server, {12, 2, 4}, {13, 3, 12});
  // blue walls
  fill_blocks(server, {4, 0, 4}, {12, 1, 5});
  fill_blocks(server, {4, 0, 12}, {12, 1, 13});
  fill_blocks(server, {4, 2, 4}, {12, 3, 5});
  fill_blocks(server, {4, 2, 12}, {12, 3, 13});
  std::cout << "Server running on port " << net::constants::port << ".\n";
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;
  auto loop_duration = Duration{};
  auto loop_time = Clock::now();
  while (!signal_status) {
    server.poll_events();
    if (server.service_game_state(
          std::chrono::duration_cast<std::chrono::duration<float>>(
            loop_duration)
            .count())) {
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
