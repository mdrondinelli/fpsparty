#include "constants.hpp"
#include "enet.hpp"
#include "game/game.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace fpsparty;

namespace {
volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }

template <typename F> void service_enet_events(enet::Host host, F &&f) {
  for (;;) {
    const auto event = host.service(0);
    switch (event.type) {
    case enet::Event_type::none:
      return;
    default:
      f(event);
      continue;
    }
  }
}

} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  auto game = game::create_game_unique({});
  std::cout << "Initialized game.\n";
  const auto enet_guard = enet::Initialization_guard{{}};
  const auto server = enet::make_server_host_unique({
      .port = constants::port,
      .max_clients = constants::max_clients,
      .max_channels = 1,
  });
  auto peers = std::vector<enet::Peer>{};
  peers.reserve(constants::max_clients);
  std::cout << "Server running on port " << constants::port << ".\n";
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;
  auto game_loop_duration = Duration{};
  auto game_loop_time = Clock::now();
  while (!signal_status) {
    service_enet_events(*server, [&](const enet::Event &e) {
      switch (e.type) {
      case enet::Event_type::connect:
        std::cout << "Got connect event.\n";
        {
          peers.emplace_back(e.peer);
          const auto player = game->create_player({});
          e.peer.set_data(static_cast<void *>(player));
        }
        break;
      case enet::Event_type::disconnect:
        std::cout << "Got disconnect event.\n";
        {
          const auto it = std::ranges::find(peers, e.peer);
          assert(it != peers.end());
          const auto player = static_cast<game::Player>(it->get_data());
          game->destroy_player(player);
          peers.erase(it);
        }
        break;
      case enet::Event_type::receive:
        std::cout << "Got receive event.\n";
        break;
      default:
        break;
      }
    });
    const auto now = Clock::now();
    game_loop_duration += now - game_loop_time;
    game_loop_time = now;
    if (game_loop_duration >=
        std::chrono::duration<float>{constants::tick_duration}) {
      game_loop_duration -= std::chrono::duration_cast<Duration>(
          std::chrono::duration<float>{constants::tick_duration});
      game->simulate(
          {.player_inputs = {}, .duration = constants::tick_duration});
    }
  }
  if (signal_status == SIGINT) {
    if (!peers.empty()) {
      std::cout << "Disconnecting " << peers.size() << " clients.\n";
      for (const auto peer : peers) {
        peer.disconnect(0);
      }
      do {
        const auto event = server->service(1000);
        if (event.type == enet::Event_type::disconnect) {
          std::cout << "Got disconnect event.\n";
          const auto it = std::ranges::find(peers, event.peer);
          assert(it != peers.end());
          peers.erase(it);
        }
      } while (!peers.empty());
    }
  }
  std::cout << "Exiting.\n";
  return 0;
}
