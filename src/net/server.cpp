#include "server.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "game/core/entity_id.hpp"
#include "game/core/sequence_number.hpp"
#include "net/message_type.hpp"
#include "serial/ostream_writer.hpp"
#include "serial/serialize.hpp"
#include "serial/span_reader.hpp"
#include "serial/span_writer.hpp"
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

void Server::disconnect() {
  for (const auto &peer : _peers) {
    peer.disconnect(0);
  }
}

void Server::send_player_join_response(enet::Peer peer,
                                       game::Entity_id player_entity_id) {
  auto writer = serial::Ostringstream_writer{};
  using serial::serialize;
  serialize<Message_type>(writer, Message_type::player_join_response);
  serialize<game::Entity_id>(writer, player_entity_id);
  peer.send(constants::player_initialization_channel_id,
            enet::create_packet_unique({
                .data = writer.stream().view().data(),
                .data_length = writer.stream().view().size(),
                .flags = enet::Packet_flag_bits::reliable,
            }));
}

void Server::send_grid_snapshot(enet::Peer peer,
                                std::span<const std::byte> state) {
  auto packet = enet::create_packet_unique({
      .data = nullptr,
      .data_length = sizeof(Message_type) + state.size(),
      .flags = enet::Packet_flag_bits::reliable,
  });
  auto writer = serial::Span_writer{std::as_writable_bytes(packet->get_data())};
  using serial::serialize;
  serialize<Message_type>(writer, Message_type::grid_snapshot);
  writer.write(state);
  peer.send(constants::grid_snapshot_channel_id, std::move(packet));
}

void Server::send_entity_snapshot(enet::Peer peer,
                                  game::Sequence_number tick_number,
                                  std::span<const std::byte> public_state,
                                  std::span<const std::byte> player_state) {
  auto packet = enet::create_packet_unique({
      .data = nullptr,
      .data_length = sizeof(Message_type) + sizeof(game::Sequence_number) +
                     sizeof(std::uint16_t) + sizeof(std::uint8_t) +
                     public_state.size() + player_state.size(),
  });
  auto writer = serial::Span_writer{std::as_writable_bytes(packet->get_data())};
  using serial::serialize;
  serialize<Message_type>(writer, Message_type::entity_snapshot);
  serialize<game::Sequence_number>(writer, tick_number);
  serialize<std::uint16_t>(writer, public_state.size());
  writer.write(public_state);
  writer.write(player_state);
  peer.send(constants::game_state_channel_id, std::move(packet));
}

std::size_t Server::get_peer_count() const noexcept { return _peers.size(); }

std::pmr::vector<enet::Peer>
Server::get_peers(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<enet::Peer>(memory_resource);
  retval.reserve(_peers.size());
  retval.insert_range(retval.begin(), _peers);
  return retval;
}

void Server::on_peer_connect(enet::Peer) {}

void Server::on_peer_disconnect(enet::Peer) {}

void Server::on_player_join_request(enet::Peer) {}

void Server::on_player_leave_request(enet::Peer, game::Entity_id) {}

void Server::on_player_input_state(enet::Peer, game::Entity_id,
                                   game::Sequence_number,
                                   const game::Humanoid_input_state &) {}

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
    case Message_type::player_join_request: {
      on_player_join_request(e.peer);
      return;
    }
    case Message_type::player_leave_request: {
      const auto player_entity_id = deserialize<game::Entity_id>(reader);
      if (!player_entity_id) {
        std::cerr << "Malformed player leave request packet.\n";
        return;
      }
      on_player_leave_request(e.peer, *player_entity_id);
      return;
    }
    case Message_type::player_input_state: {
      const auto player_entity_id = deserialize<game::Entity_id>(reader);
      if (!player_entity_id) {
        std::cerr << "Malformed player input state packet.\n";
        return;
      }
      const auto input_sequence_number =
          deserialize<game::Sequence_number>(reader);
      if (!input_sequence_number) {
        std::cerr << "Malformed player input state packet.\n";
        return;
      }
      const auto input_state = deserialize<game::Humanoid_input_state>(reader);
      if (!input_state) {
        std::cerr << "Malformed player input state packet.\n";
        return;
      }
      on_player_input_state(e.peer, *player_entity_id, *input_sequence_number,
                            *input_state);
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
} // namespace fpsparty::net
