#include "server.hpp"

#include "game/humanoid.hpp"
#include "game/item.hpp"
#include "game/player.hpp"
#include "serial/ostream_writer.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <span>
#include <tracy/Tracy.hpp>
#include <vector>

namespace fpsparty::server {
namespace {
struct Peer_node {
  std::vector<game::Entity_handle<game::Player>> players;
};

void erase_player(
  game::Entity_world &world, game::Entity_handle<game::Player> player_handle) {
  auto const players = world.get_all<game::Player>();
  auto const player_it = players.find(player_handle);
  if (player_it == players.end()) {
    return;
  }
  auto &player = (*player_it).entity;
  auto const humanoids = world.get_all<game::Humanoid>();
  auto const humanoid_it = humanoids.find(player.humanoid);
  if (humanoid_it != humanoids.end()) {
    humanoids.erase(humanoid_it);
  }
  players.erase(player_it);
}
} // namespace

Server::Server(Server_create_info const &info)
    : net::Server{info.net_info},
      _game{info.game_info},
      _tick_duration{info.tick_duration} {}

bool Server::update(float duration) {
  _tick_timer -= duration;
  if (_tick_timer <= 0.0f) {
    _tick_timer += _tick_duration;
    {
      ZoneScopedN("Game tick");
      _game.tick(_tick_duration);
    }
    ++_tick_number;
    return true;
  }
  return false;
}

void Server::broadcast_game_state() {
  ZoneScoped;
  using serial::serialize;
  auto grid_state_writer = serial::Ostringstream_writer{};
  _game.get_grid().dump(grid_state_writer);
  auto public_state_writer = serial::Ostringstream_writer{};
  auto &world = _game.get_entities();
  auto const humanoids = world.get_all<game::Humanoid>();
  auto const items = world.get_all<game::Item>();
  serialize<std::uint32_t>(
    public_state_writer,
    static_cast<std::uint32_t>(humanoids.size() + items.size()));
  for (auto [humanoid, handle] : humanoids) {
    serialize<game::Entity_type>(
      public_state_writer, game::Entity_type::humanoid);
    serialize<net::Entity_id>(public_state_writer, handle.id);
    game::Entity_traits<game::Humanoid>::dump(public_state_writer, humanoid);
  }
  for (auto [item, handle] : items) {
    serialize<game::Entity_type>(public_state_writer, game::Entity_type::item);
    serialize<net::Entity_id>(public_state_writer, handle.id);
    game::Entity_traits<game::Item>::dump(public_state_writer, item);
  }
  auto const players = world.get_all<game::Player>();
  for (auto const &peer : get_peers()) {
    auto player_state_writer = serial::Ostringstream_writer{};
    auto const peer_node = static_cast<Peer_node *>(peer.get_data());
    std::erase_if(peer_node->players, [&](auto const player_handle) {
      return players.find(player_handle) == players.end();
    });
    serialize<std::uint32_t>(
      player_state_writer,
      static_cast<std::uint32_t>(peer_node->players.size()));
    for (auto const player_handle : peer_node->players) {
      auto &player = *world.get(player_handle);
      if (player.humanoid && !world.get(player.humanoid)) {
        player.humanoid = {};
      }
      serialize<game::Entity_type>(
        player_state_writer, game::Entity_type::player);
      serialize<net::Entity_id>(player_state_writer, player_handle.id);
      game::Entity_traits<game::Player>::dump(player_state_writer, player);
    }
    net::Server::send_world_snapshot(
      peer,
      _tick_number,
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
}

game::Game const &Server::get_game() const noexcept { return _game; }

game::Game &Server::get_game() noexcept { return _game; }

net::Sequence_number Server::get_tick_number() const noexcept {
  return _tick_number;
}

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

void Server::on_player_join_request(
  enet::Peer peer, net::Player_join_request_id request_id) {
  auto const peer_node = static_cast<Peer_node *>(peer.get_data());
  auto const player_handle =
    _game.get_entities().emplace<game::Player>().handle;
  peer_node->players.push_back(player_handle);
  send_player_join_response(peer, request_id, player_handle.id);
}

void Server::on_player_leave_request(
  enet::Peer peer, net::Entity_id player_entity_id) {
  auto const peer_node = static_cast<Peer_node *>(peer.get_data());
  auto const it = std::ranges::find(
    peer_node->players,
    player_entity_id,
    &game::Entity_handle<game::Player>::id);
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
  auto const peer_node = static_cast<Peer_node *>(peer.get_data());
  for (auto it = peer_node->players.begin(); it != peer_node->players.end();) {
    auto const player = _game.get_entities().get(*it);
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

} // namespace fpsparty::server
