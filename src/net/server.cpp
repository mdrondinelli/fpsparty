#include "server.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "net/message_type.hpp"
#include "serial/ostream_writer.hpp"
#include "serial/serialize.hpp"
#include "serial/span_reader.hpp"
#include <span>

namespace fpsparty::net {
Server::Server(const Server_create_info &create_info)
    : _host{enet::make_server_host_unique({
          .port = create_info.port,
          .max_clients = create_info.max_clients,
          .max_channels = 1,
          .incoming_bandwidth = create_info.incoming_bandwidth,
          .outgoing_bandwidth = create_info.outgoing_bandwidth,
      })},
      _game{create_info.game} {}

void Server::poll_events() {
  _host->service_each([&](const enet::Event &e) {
    switch (e.type) {
    case enet::Event_type::connect: {
      _peers.emplace_back(e.peer);
      on_peer_connect(e.peer);
      return;
    }
    case enet::Event_type::disconnect: {
      on_peer_disconnect(e.peer);
      const auto it = std::ranges::find(_peers, e.peer);
      assert(it != _peers.end());
      _peers.erase(it);
      return;
    }
    case enet::Event_type::receive: {
      auto reader = serial::Span_reader{std::as_bytes(e.packet->get_data())};
      const auto message_type = serial::deserialize<Message_type>(reader);
      if (!message_type) {
        return;
      }
      switch (*message_type) {
      case Message_type::player_input_state: {
        const auto player_input_state =
            serial::deserialize<game::Player::Input_state>(reader);
        if (player_input_state) {
          on_player_input_state(e.peer, *player_input_state);
        }
        break;
      }
        return;
      }
    }
    default:
      return;
    }
  });
}

void Server::disconnect() {
  for (const auto &peer : _peers) {
    peer.disconnect(0);
  }
  do {
    const auto event = _host->service(100);
    if (event.type == enet::Event_type::disconnect) {
      on_peer_disconnect(event.peer);
      const auto it = std::ranges::find(_peers, event.peer);
      assert(it != _peers.end());
      _peers.erase(it);
    }
  } while (!_peers.empty());
}

void Server::broadcast_game_state() {
  auto packet_writer = serial::Ostringstream_writer{};
  serial::serialize<Message_type>(packet_writer, Message_type::game_state);
  _game.snapshot(packet_writer);
  _host->broadcast(
      constants::snapshot_channel_id,
      enet::create_packet_unique(
          {.data = packet_writer.stream().view().data(),
           .data_length = packet_writer.stream().view().length()}));
}
} // namespace fpsparty::net
