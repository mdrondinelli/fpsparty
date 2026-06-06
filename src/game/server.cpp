#include "server.hpp"

#include "game/humanoid.hpp"
#include "game/player.hpp"
#include "game/projectile.hpp"
#include "serial/ostream_writer.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <span>
#include <vector>

namespace fpsparty::game {
namespace {
struct Peer_node {
  std::vector<Entity_handle<Player>> players;
};

void erase_player(Entity_world &world, Entity_handle<Player> player_handle) {
  auto const *player = world.get_entity(player_handle);
  if (!player) {
    return;
  }
  if (world.get_entity(player->humanoid)) {
    world.erase_entity(player->humanoid);
  }
  world.erase_entity(player_handle);
}
} // namespace

Server::Server(Server_create_info const &info)
    : net::Server{info.net_info},
      _game{info.game_info},
      _tick_duration{info.tick_duration} {}

bool Server::service_game_state(float duration) {
  _tick_timer -= duration;
  if (_tick_timer <= 0.0f) {
    _tick_timer += _tick_duration;
    _game.tick(_tick_duration);
    return true;
  }
  return false;
}

void Server::broadcast_game_state() {
  using serial::serialize;

  auto grid_state_writer = serial::Ostringstream_writer{};
  _game.get_grid().dump(grid_state_writer);

  auto public_state_writer = serial::Ostringstream_writer{};
  auto &world = _game.get_entities();
  auto const humanoids = world.get_entities_with_handles<Humanoid>();
  auto const projectiles = world.get_entities_with_handles<Projectile>();
  serialize<std::uint32_t>(
    public_state_writer,
    static_cast<std::uint32_t>(humanoids.size() + projectiles.size()));
  for (auto [humanoid, handle] : humanoids) {
    serialize<Entity_type>(public_state_writer, Entity_type::humanoid);
    serialize<net::Entity_id>(public_state_writer, handle.id);
    Entity_traits<Humanoid>::dump(public_state_writer, humanoid);
  }
  for (auto [projectile, handle] : projectiles) {
    serialize<Entity_type>(public_state_writer, Entity_type::projectile);
    serialize<net::Entity_id>(public_state_writer, handle.id);
    Entity_traits<Projectile>::dump(public_state_writer, projectile);
  }

  for (auto const &peer : get_peers()) {
    auto player_state_writer = serial::Ostringstream_writer{};
    auto *peer_node = static_cast<Peer_node *>(peer.get_data());
    std::erase_if(peer_node->players, [&](auto const handle) {
      return !world.get_entity(handle);
    });
    serialize<std::uint32_t>(
      player_state_writer,
      static_cast<std::uint32_t>(peer_node->players.size()));
    for (auto const player_handle : peer_node->players) {
      auto &player = *world.get_entity(player_handle);
      if (player.humanoid && !world.get_entity(player.humanoid)) {
        player.humanoid = {};
      }
      serialize<Entity_type>(player_state_writer, Entity_type::player);
      serialize<net::Entity_id>(player_state_writer, player_handle.id);
      Entity_traits<Player>::dump(player_state_writer, player);
    }

    net::Server::send_world_snapshot(
      peer,
      std::as_bytes(
        std::span{
          grid_state_writer.stream().view().data(),
          grid_state_writer.stream().view().size(),
        }),
      std::as_bytes(
        std::span{
          public_state_writer.stream().view().data(),
          public_state_writer.stream().view().size(),
        }),
      std::as_bytes(
        std::span{
          player_state_writer.stream().view().data(),
          player_state_writer.stream().view().size(),
        }));
  }
  net::Server::flush();
}

Game const &Server::get_game() const noexcept { return _game; }

Game &Server::get_game() noexcept { return _game; }

void Server::on_peer_connect(enet::Peer peer) {
  std::cout << "Peer connected.\n";
  peer.set_data(new Peer_node);
}

void Server::on_peer_disconnect(enet::Peer peer) {
  std::cout << "Peer disconnected.\n";
  auto const peer_node =
    std::unique_ptr<Peer_node>{static_cast<Peer_node *>(peer.get_data())};
  for (auto const player_handle : peer_node->players) {
    erase_player(_game.get_entities(), player_handle);
  }
}

void Server::on_player_join_request(enet::Peer peer) {
  auto *peer_node = static_cast<Peer_node *>(peer.get_data());
  auto const player_handle = _game.create_player();
  peer_node->players.push_back(player_handle);
  send_player_join_response(peer, player_handle.id);
}

void Server::on_player_leave_request(
  enet::Peer peer, net::Entity_id player_entity_id) {
  auto *peer_node = static_cast<Peer_node *>(peer.get_data());
  auto const it = std::ranges::find(
    peer_node->players, player_entity_id, &Entity_handle<Player>::id);
  if (it == peer_node->players.end()) {
    return;
  }
  erase_player(_game.get_entities(), *it);
  peer_node->players.erase(it);
}

void Server::on_player_input_state(
  enet::Peer peer,
  net::Entity_id player_entity_id,
  net::Input_state const &input_state) {
  auto *peer_node = static_cast<Peer_node *>(peer.get_data());
  for (auto it = peer_node->players.begin(); it != peer_node->players.end();) {
    auto *player = _game.get_entities().get_entity(*it);
    if (!player) {
      it = peer_node->players.erase(it);
      continue;
    }
    if (it->id == player_entity_id) {
      player->input_state = input_state;
      return;
    }
    ++it;
  }
}

} // namespace fpsparty::game
