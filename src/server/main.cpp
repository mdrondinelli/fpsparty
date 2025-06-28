#include "constants.hpp"
#include "enet.hpp"
#include "game_authority/game.hpp"
#include "net/constants.hpp"
#include "net/server.hpp"
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>

using namespace fpsparty;

namespace {
volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }

class Server : public net::Server {
public:
  struct Create_info {
    std::uint16_t port;
    std::size_t max_clients;
    std::uint32_t incoming_bandwidth{};
    std::uint32_t outgoing_bandwidth{};
  };

  explicit Server(const Create_info &create_info)
      : net::Server{{.port = create_info.port,
                     .max_clients = create_info.max_clients,
                     .incoming_bandwidth = create_info.incoming_bandwidth,
                     .outgoing_bandwidth = create_info.outgoing_bandwidth}},
        _game{game::create_game_unique({})} {}

  bool service_game_state(float duration) {
    _tick_timer -= duration;
    if (_tick_timer <= 0.0f) {
      _tick_timer += constants::tick_duration;
      const auto players = _game->get_humanoids();
      for (const auto &player : players) {
        if (player.is_input_stale()) {
          player.increment_input_sequence_number();
        }
      }
      _game->simulate({.duration = constants::tick_duration});
      for (const auto &player : players) {
        player.mark_input_stale();
      }
      return true;
    } else {
      return false;
    }
  }

  void broadcast_game_state() { net::Server::broadcast_game_state(*_game); }

  constexpr game::Game get_game() const noexcept { return *_game; }

protected:
  void on_peer_connect(enet::Peer peer) override {
    std::cout << "Peer connected.\n";
    const auto player = _game->create_humanoid({});
    peer.set_data(static_cast<void *>(player));
    send_player_id(peer, player.get_network_id());
  }

  void on_peer_disconnect(enet::Peer peer) override {
    std::cout << "Peer disconnected.\n";
    const auto player = static_cast<game::Humanoid>(peer.get_data());
    _game->destroy_humanoid(player);
  }

  void on_player_input_state(enet::Peer peer,
                             const game_core::Humanoid_input_state &input_state,
                             std::uint16_t input_sequence_number) override {
    const auto player = static_cast<game::Humanoid>(peer.get_data());
    player.set_input_state(input_state, input_sequence_number, true);
  }

private:
  game::Unique_game _game{};
  float _tick_timer{};
};
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto enet_guard = enet::Initialization_guard{{}};
  auto server = Server{{
      .port = net::constants::port,
      .max_clients = net::constants::max_clients,
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
    } while (server.get_game().get_humanoid_count() > 0);
  }
  std::cout << "Exiting.\n";
  return 0;
}
