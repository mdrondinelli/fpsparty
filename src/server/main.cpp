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
      .port = net::constants::port,
      .max_clients = net::constants::max_clients,
      .tick_duration = constants::tick_duration,
  }};
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
