#include "server.hpp"
#include "game/server/humanoid.hpp"
#include "game/server/player.hpp"
#include "game/server/projectile.hpp"
#include "serial/ostream_writer.hpp"
#include <iostream>

namespace fpsparty::game {
namespace {
struct Peer_node {
  std::vector<Player *> players;
};
} // namespace

Server::Server(const Server_create_info &info)
    : net::Server{info.net_info},
      _game{info.game_info},
      _tick_duration{info.tick_duration} {}

bool Server::service_game_state(float duration) {
  _tick_timer -= duration;
  if (_tick_timer <= 0.0f) {
    _tick_timer += _tick_duration;
    _game.tick(_tick_duration);
    return true;
  } else {
    return false;
  }
}

void Server::broadcast_game_state() {
  using serial::serialize;
  auto public_state_writer = serial::Ostringstream_writer{};
  const auto humanoid_dumper = Humanoid_dumper{};
  const auto projectile_dumper = game::Projectile_dumper{};
  const auto public_state_dumpers = std::array<const game::Entity_dumper *, 2>{
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
      serialize<net::Entity_id>(player_state_writer, player->get_entity_id());
      Player_dumper{}.dump_entity(player_state_writer, *player);
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

const Game &Server::get_game() const noexcept { return _game; }

Game &Server::get_game() noexcept { return _game; }

void Server::on_peer_connect(enet::Peer peer) {
  std::cout << "Peer connected.\n";
  peer.set_data(new Peer_node);
  auto writer = serial::Ostringstream_writer{};
  _game.get_grid().dump(writer);
  send_grid_snapshot(peer, std::as_bytes(std::span{
                               writer.stream().view().data(),
                               writer.stream().view().size(),
                           }));
}

void Server::on_peer_disconnect(enet::Peer peer) {
  std::cout << "Peer disconnected.\n";
  const auto peer_node =
      std::unique_ptr<Peer_node>{static_cast<Peer_node *>(peer.get_data())};
  for (const auto &player : peer_node->players) {
    if (const auto humanoid = player->get_humanoid()) {
      _game.get_entities().remove(humanoid);
    }
    _game.get_entities().remove(player);
  }
}

void Server::on_player_join_request(enet::Peer peer) {
  const auto peer_node = static_cast<Peer_node *>(peer.get_data());
  auto player = _game.create_player({});
  peer_node->players.emplace_back(player.get());
  send_player_join_response(peer, player->get_entity_id());
  _game.get_entities().add(std::move(player));
}

void Server::on_player_leave_request(enet::Peer peer,
                                     net::Entity_id player_entity_id) {
  const auto peer_node = static_cast<Peer_node *>(peer.get_data());
  for (auto it = peer_node->players.begin(); it != peer_node->players.end();
       ++it) {
    if ((*it)->get_entity_id() == player_entity_id) {
      if (const auto humanoid = (*it)->get_humanoid()) {
        _game.get_entities().remove(humanoid);
      }
      _game.get_entities().remove(*it);
      peer_node->players.erase(it);
      return;
    }
  }
}

void Server::on_player_input_state(enet::Peer peer,
                                   net::Entity_id player_entity_id,
                                   net::Sequence_number input_sequence_number,
                                   const net::Input_state &input_state) {
  const auto peer_node = static_cast<Peer_node *>(peer.get_data());
  for (const auto &player : peer_node->players) {
    if (player->get_entity_id() == player_entity_id) {
      player->set_input_state(input_state, input_sequence_number);
    }
  }
}
} // namespace fpsparty::game
