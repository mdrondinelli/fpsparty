#include "constants.hpp"
#include "enet.hpp"
#include "net/constants.hpp"
#include "server/server.hpp"
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <tracy/Tracy.hpp>

using namespace fpsparty;

namespace {
std::sig_atomic_t volatile signal_status{};
void handle_signal(int signal) { signal_status = signal; }

void set_block(
  server::Server &server,
  math::ivec3 const &pos,
  game::Block block,
  int data = 0) {
  server.get_game().get_grid().set_block(pos, block, data);
}

void fill_blocks(
  server::Server &server,
  math::ivec3 const &min,
  math::ivec3 const &max,
  game::Block block,
  int data = 0) {
  server.get_game().get_grid().fill({min, max}, block, data);
}
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  auto const enet_guard = enet::Initialization_guard{{}};
  auto server = server::Server{{
    .net_info =
      {
        .port = net::constants::port,
        .max_clients = net::constants::max_clients,
      },
    .game_info =
      {
        .grid_bounds =
          math::ibox3{
                math::ivec3{-32, 0, -32},
                math::ivec3{31, 15, 31},
              },
        .initial_solar_time = 8ull * 60ull * 60ull * 1'000'000ull,
      },
    .tick_duration = constants::tick_duration,
  }};
  // floor
  fill_blocks(server, {-16, 0, -16}, {15, 0, 15}, game::Block::dirt);
  // doorway
  fill_blocks(server, {-1, 1, 1}, {-1, 3, 1}, game::Block::stone);
  fill_blocks(server, {1, 1, 1}, {1, 3, 1}, game::Block::stone);
  set_block(server, {0, 3, 1}, game::Block::stone);
  std::cout << "Server running on port " << net::constants::port << ".\n";
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;
  auto loop_duration = Duration{};
  auto loop_time = Clock::now();
  FrameMark;
  while (!signal_status) {
    server.poll_events();
    auto const duration =
      std::chrono::duration_cast<std::chrono::duration<float>>(loop_duration)
        .count();
    if (server.update(duration)) {
      server.broadcast_game_state();
      FrameMark;
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
