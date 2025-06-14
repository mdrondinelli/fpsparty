#include "constants.hpp"
#include "enet.hpp"
#include "game/game.hpp"
#include "net/server.hpp"
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

class Server : public net::Server {
public:
  explicit Server(const net::Server_create_info &create_info)
      : net::Server{create_info} {}

protected:
  void on_peer_connect(enet::Peer peer) override {
    std::cout << "Peer connected.\n";
    const auto player = get_game().create_player({});
    peer.set_data(static_cast<void *>(player));
  }

  void on_peer_disconnect(enet::Peer peer) override {
    std::cout << "Peer disconnected.\n";
    const auto player = static_cast<game::Player>(peer.get_data());
    get_game().destroy_player(player);
  }

  void
  on_player_input_state(enet::Peer peer,
                        const game::Player::Input_state &input_state) override {
    const auto player = static_cast<game::Player>(peer.get_data());
    player.set_input_state(input_state);
  }
};
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  auto game = game::create_game_unique({});
  std::cout << "Initialized game.\n";
  const auto enet_guard = enet::Initialization_guard{{}};
  auto server = Server{{
      .port = constants::port,
      .max_clients = constants::max_clients,
      .game = *game,
  }};
  auto peers = std::vector<enet::Peer>{};
  peers.reserve(constants::max_clients);
  std::cout << "Server running on port " << constants::port << ".\n";
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;
  auto game_loop_duration = Duration{};
  auto game_loop_time = Clock::now();
  while (!signal_status) {
    server.poll_events();
    const auto now = Clock::now();
    game_loop_duration += now - game_loop_time;
    game_loop_time = now;
    if (game_loop_duration >=
        std::chrono::duration<float>{constants::tick_duration}) {
      game_loop_duration -= std::chrono::duration_cast<Duration>(
          std::chrono::duration<float>{constants::tick_duration});
      game->simulate({.duration = constants::tick_duration});
      server.broadcast_game_state();
    }
  }
  if (signal_status == SIGINT) {
    server.disconnect();
  }
  std::cout << "Exiting.\n";
  return 0;
}
