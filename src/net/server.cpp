#include "server.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "net/message_type.hpp"
#include "serial/ostream_writer.hpp"
#include "serial/serialize.hpp"
#include "serial/span_reader.hpp"
#include <iostream>
#include <span>

namespace fpsparty::net {
Server::Server(const Create_info &create_info)
    : _host{enet::make_server_host_unique({
          .port = create_info.port,
          .max_clients = create_info.max_clients,
          .max_channels = constants::max_channels,
          .incoming_bandwidth = create_info.incoming_bandwidth,
          .outgoing_bandwidth = create_info.outgoing_bandwidth,
      })} {}

void Server::poll_events() {
  _host->service_each([&](const enet::Event &e) { handle_event(e); });
}

void Server::wait_events(std::uint32_t timeout) {
  auto e = _host->service(timeout);
  while (e.type != enet::Event_type::none) {
    handle_event(e);
    e = _host->service(0);
  }
}

void Server::handle_event(const enet::Event &e) {
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
    using serial::deserialize;
    auto reader = serial::Span_reader{std::as_bytes(e.packet->get_data())};
    const auto message_type = deserialize<Message_type>(reader);
    if (!message_type) {
      return;
    }
    switch (*message_type) {
    case Message_type::player_input_state: {
      const auto input_state =
          deserialize<game_core::Humanoid_input_state>(reader);
      if (!input_state) {
        std::cerr << "Malformed player input state packet.\n";
        return;
      }
      const auto input_sequence_number = deserialize<std::uint16_t>(reader);
      if (!input_sequence_number) {
        std::cerr << "Malformed player input state packet.\n";
        return;
      }
      on_player_input_state(e.peer, *input_state, *input_sequence_number);
      return;
    }
    default:
      std::cerr << "Received packet with unexpected message type.\n";
      return;
    }
  }
  default:
    return;
  }
}

void Server::disconnect() {
  for (const auto &peer : _peers) {
    peer.disconnect(0);
  }
}

void Server::send_player_id(enet::Peer peer, std::uint32_t player_id) {
  auto writer = serial::Ostringstream_writer{};
  using serial::serialize;
  serialize<Message_type>(writer, Message_type::player_id);
  serialize<std::uint32_t>(writer, player_id);
  peer.send(
      constants::player_id_channel_id,
      enet::create_packet_unique({.data = writer.stream().view().data(),
                                  .data_length = writer.stream().view().size(),
                                  .flags = enet::Packet_flag_bits::reliable}));
}

void Server::broadcast_game_state(game::Game game) {
  auto packet_writer = serial::Ostringstream_writer{};
  serial::serialize<Message_type>(packet_writer, Message_type::game_state);
  game.snapshot(packet_writer);
  _host->broadcast(
      constants::game_state_channel_id,
      enet::create_packet_unique(
          {.data = packet_writer.stream().view().data(),
           .data_length = packet_writer.stream().view().length()}));
}
} // namespace fpsparty::net
