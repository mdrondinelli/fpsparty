#include "constants.hpp"
#include "enet.hpp"
#include "game/core/entity_id.hpp"
#include "game/core/entity_type.hpp"
#include "game/core/sequence_number.hpp"
#include "game/server/game.hpp"
#include "game/server/player.hpp"
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
    using serial::serialize;
    auto public_state_writer = serial::Ostringstream_writer{};
    const auto humanoid_dumper = game::Humanoid_dumper{};
    const auto projectile_dumper = game::Projectile_dumper{};
    const auto public_state_dumpers =
        std::array<const game::Entity_dumper *, 2>{
            &humanoid_dumper,
            &projectile_dumper,
        };
    _game.get_entities().dump({
        .writer = &public_state_writer,
        .dumpers = public_state_dumpers,
    });
    for (const auto &peer : get_peers()) {
      auto player_state_writer = serial::Ostringstream_writer{};
      const auto peer_node = static_cast<Peer_node *>(peer.get_data());
      const auto peer_player_count = peer_node->players.size();
      serialize<std::uint32_t>(player_state_writer, peer_player_count);
      for (const auto &player : peer_node->players) {
        serialize<game::Entity_type>(player_state_writer,
                                     game::Entity_type::player);
        serialize<game::Entity_id>(player_state_writer,
                                   player->get_entity_id());
        game::Player_dumper{}.dump_entity(player_state_writer, *player);
      }
      net::Server::send_entity_snapshot(
          peer, _game.get_tick_number(),
          std::as_bytes(std::span{
              public_state_writer.stream().view().data(),
              public_state_writer.stream().view().size(),
          }),
          std::as_bytes(std::span{
              player_state_writer.stream().view().data(),
              player_state_writer.stream().view().size(),
          }));
    }
  }

  std::size_t get_peer_count() const noexcept {
    return net::Server::get_peer_count();
  }

protected:
  void on_peer_connect(enet::Peer peer) override {
    std::cout << "Peer connected.\n";
    peer.set_data(new Peer_node);
    return;
    const auto &grid = _game.get_grid();
    auto writer = serial::Ostringstream_writer{};
    grid.dump(writer);
    send_grid_snapshot(peer,
                       std::as_bytes(std::span{writer.stream().view().data(),
                                               writer.stream().view().size()}));
  }

  void on_peer_disconnect(enet::Peer peer) override {
    std::cout << "Peer disconnected.\n";
    const auto peer_node =
        std::unique_ptr<Peer_node>{static_cast<Peer_node *>(peer.get_data())};
    for (const auto &player : peer_node->players) {
      const auto humanoid = player->get_humanoid().lock();
      if (humanoid) {
        _game.get_entities().remove(humanoid);
      }
      _game.get_entities().remove(player);
    }
  }

  void on_player_join_request(enet::Peer peer) override {
    const auto peer_node = static_cast<Peer_node *>(peer.get_data());
    const auto player = _game.create_player({});
    peer_node->players.emplace_back(player);
    _game.get_entities().add(player);
    send_player_join_response(peer, player->get_entity_id());
  }

  void on_player_leave_request(enet::Peer peer,
                               game::Entity_id player_entity_id) override {
    const auto peer_node = static_cast<Peer_node *>(peer.get_data());
    for (auto it = peer_node->players.begin();
         it != peer_node->players.end();) {
      if ((*it)->get_entity_id() == player_entity_id) {
        const auto humanoid = (*it)->get_humanoid().lock();
        if (humanoid) {
          _game.get_entities().remove(humanoid);
        }
        _game.get_entities().remove(*it);
        it = peer_node->players.erase(it);
      } else {
        ++it;
      }
    }
  }

  void on_player_input_state(
      enet::Peer peer, game::Entity_id player_entity_id,
      game::Sequence_number input_sequence_number,
      const game::Humanoid_input_state &input_state) override {
    const auto peer_node = static_cast<Peer_node *>(peer.get_data());
    for (const auto &player : peer_node->players) {
      if (player->get_entity_id() == player_entity_id) {
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
