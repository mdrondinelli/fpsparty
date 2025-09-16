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
volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto enet_guard = enet::Initialization_guard{{}};
  auto server = game::Server{{
    .net_info =
      {
        .port = net::constants::port,
        .max_clients = net::constants::max_clients,
      },
    .game_info = {.grid_info = {.width = 256, .height = 256, .depth = 256}},
    .tick_duration = constants::tick_duration,
  }};
  // green floor
  server.get_game().get_grid().fill(
    game::Axis::y,
    0,
    {
      Eigen::Vector2i{0, 0},
      Eigen::Vector2i{16, 16},
    });
  server.get_game().get_grid().fill(
    game::Axis::y,
    1,
    {
      Eigen::Vector2i{3, 3},
      Eigen::Vector2i{13, 13},
    });
  server.get_game().get_grid().fill(
    game::Axis::y,
    1,
    {
      Eigen::Vector2i{4, 4},
      Eigen::Vector2i{12, 12},
    },
    false);
  // red walls
  server.get_game().get_grid().fill(
    game::Axis::x,
    4,
    {
      Eigen::Vector2i{0, 4},
      Eigen::Vector2i{1, 12},
    });
  server.get_game().get_grid().fill(
    game::Axis::x,
    12,
    {
      Eigen::Vector2i{0, 4},
      Eigen::Vector2i{1, 12},
    });
  server.get_game().get_grid().fill(
    game::Axis::x,
    4,
    {
      Eigen::Vector2i{2, 4},
      Eigen::Vector2i{3, 12},
    });
  server.get_game().get_grid().fill(
    game::Axis::x,
    12,
    {
      Eigen::Vector2i{2, 4},
      Eigen::Vector2i{3, 12},
    });
  // blue walls
  server.get_game().get_grid().fill(
    game::Axis::z,
    4,
    {
      Eigen::Vector2i{4, 0},
      Eigen::Vector2i{12, 1},
    });
  server.get_game().get_grid().fill(
    game::Axis::z,
    12,
    {
      Eigen::Vector2i{4, 0},
      Eigen::Vector2i{12, 1},
    });
  server.get_game().get_grid().fill(
    game::Axis::z,
    4,
    {
      Eigen::Vector2i{4, 2},
      Eigen::Vector2i{12, 3},
    });
  server.get_game().get_grid().fill(
    game::Axis::z,
    12,
    {
      Eigen::Vector2i{4, 2},
      Eigen::Vector2i{12, 3},
    });
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
    const auto now = Clock::now();
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
