#include "constants.hpp"
#include "enet.hpp"
#include "game/core/game_object_id.hpp"
#include "game/core/sequence_number.hpp"
#include "game/server/game.hpp"
#include "net/constants.hpp"
#include "net/server.hpp"
#include "serial/ostream_writer.hpp"
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
        _game{{}} {}

  bool service_game_state(float duration) {
    _tick_timer -= duration;
    if (_tick_timer <= 0.0f) {
      _tick_timer += constants::tick_duration;
      _game.tick(constants::tick_duration);
      return true;
    } else {
      return false;
    }
  }

  void broadcast_game_state() {
    auto public_state_writer = serial::Ostringstream_writer{};
    _game.get_world().dump(public_state_writer);
    for (const auto &peer : get_peers()) {
      auto player_state_writer = serial::Ostringstream_writer{};
      const auto peer_node = static_cast<Peer_node *>(peer.get_data());
      const auto peer_player_count = peer_node->players.size();
      for (const auto &player : peer_node->players) {
        player->dump(player_state_writer);
      }
      net::Server::send_game_state(
          peer, _game.get_tick_number(),
          std::as_bytes(std::span{
              public_state_writer.stream().view().data(),
              public_state_writer.stream().view().size(),
          }),
          std::as_bytes(std::span{
              player_state_writer.stream().view().data(),
              player_state_writer.stream().view().size(),
          }),
          peer_player_count);
    }
  }

  std::size_t get_peer_count() const noexcept {
    return net::Server::get_peer_count();
  }

protected:
  void on_peer_connect(enet::Peer peer) override {
    std::cout << "Peer connected.\n";
    peer.set_data(new Peer_node);
  }

  void on_peer_disconnect(enet::Peer peer) override {
    std::cout << "Peer disconnected.\n";
    const auto peer_node =
        std::unique_ptr<Peer_node>{static_cast<Peer_node *>(peer.get_data())};
    for (const auto &player : peer_node->players) {
      const auto humanoid = player->get_humanoid().lock();
      if (humanoid) {
        _game.get_world().remove(humanoid);
      }
      _game.get_world().remove(player);
    }
  }

  void on_player_join_request(enet::Peer peer) override {
    const auto peer_node = static_cast<Peer_node *>(peer.get_data());
    const auto player = _game.create_player({});
    peer_node->players.emplace_back(player);
    _game.get_world().add(player);
    send_player_join_response(peer, player->get_game_object_id());
  }

  void
  on_player_leave_request(enet::Peer peer,
                          game::Game_object_id player_game_object_id) override {
    const auto peer_node = static_cast<Peer_node *>(peer.get_data());
    for (auto it = peer_node->players.begin();
         it != peer_node->players.end();) {
      if ((*it)->get_game_object_id() == player_game_object_id) {
        const auto humanoid = (*it)->get_humanoid().lock();
        if (humanoid) {
          _game.get_world().remove(humanoid);
        }
        _game.get_world().remove(*it);
        it = peer_node->players.erase(it);
      } else {
        ++it;
      }
    }
  }

  void on_player_input_state(
      enet::Peer peer, game::Game_object_id player_game_object_id,
      game::Sequence_number input_sequence_number,
      const game::Humanoid_input_state &input_state) override {
    const auto peer_node = static_cast<Peer_node *>(peer.get_data());
    for (const auto &player : peer_node->players) {
      if (player->get_game_object_id() == player_game_object_id) {
        player->set_input_state(input_state, input_sequence_number);
      }
    }
  }

private:
  struct Peer_node {
    std::vector<rc::Strong<game::Player>> players;
  };

  game::Game _game{{}};
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
    } while (server.get_peer_count() > 0);
  }
  std::cout << "Exiting.\n";
  return 0;
}
