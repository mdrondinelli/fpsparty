#include "client.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "net/message_type.hpp"
#include "serial/ostream_writer.hpp"
#include "serial/serialize.hpp"
#include "serial/span_reader.hpp"
#include <iostream>

namespace fpsparty::net {
Client::Client(const Create_info &create_info)
    : _host{enet::make_client_host_unique(
          {.max_channels = constants::max_channels,
           .incoming_bandwidth = create_info.incoming_bandwidth,
           .outgoing_bandwidth = create_info.outgoing_bandwidth})} {}

void Client::poll_events() {
  _host->service_each([&](const enet::Event &e) { handle_event(e); });
}

void Client::wait_events(std::uint32_t timeout) {
  auto e = _host->service(timeout);
  while (e.type != enet::Event_type::none) {
    handle_event(e);
    e = _host->service(0);
  }
}

void Client::connect(const enet::Address &address) {
  assert(!is_connecting() && !is_connected());
  _server = _host->connect(address, constants::max_channels, 0);
  _connecting = true;
}

bool Client::is_connecting() const noexcept { return _connecting; }

bool Client::is_connected() const noexcept {
  return _server != nullptr && !_connecting;
}

void Client::send_player_join_request() {
  auto packet_writer = serial::Ostringstream_writer{};
  using serial::serialize;
  serialize<net::Message_type>(packet_writer,
                               net::Message_type::player_join_request);
  _server.send(constants::player_initialization_channel_id,
               enet::create_packet_unique({
                   .data = packet_writer.stream().view().data(),
                   .data_length = packet_writer.stream().view().length(),
                   .flags = enet::Packet_flag_bits::reliable,
               }));
}

void Client::send_player_leave_request(std::uint32_t player_network_id) {
  auto packet_writer = serial::Ostringstream_writer{};
  using serial::serialize;
  serialize<net::Message_type>(packet_writer,
                               net::Message_type::player_leave_request);
  serialize<std::uint32_t>(packet_writer, player_network_id);
  _server.send(constants::player_initialization_channel_id,
               enet::create_packet_unique({
                   .data = packet_writer.stream().view().data(),
                   .data_length = packet_writer.stream().view().length(),
                   .flags = enet::Packet_flag_bits::reliable,
               }));
}

void Client::send_player_input_state(
    std::uint32_t player_network_id, std::uint16_t input_sequence_number,
    const game::Humanoid_input_state &input_state) {
  auto packet_writer = serial::Ostringstream_writer{};
  using serial::serialize;
  serialize<net::Message_type>(packet_writer,
                               net::Message_type::player_input_state);
  serialize<game::Humanoid_input_state>(packet_writer, input_state);
  serialize<std::uint32_t>(packet_writer, player_network_id);
  serialize<std::uint16_t>(packet_writer, input_sequence_number);
  _server.send(constants::player_input_state_channel_id,
               enet::create_packet_unique({
                   .data = packet_writer.stream().view().data(),
                   .data_length = packet_writer.stream().view().length(),
               }));
}

void Client::handle_event(const enet::Event &e) {
  switch (e.type) {
  case enet::Event_type::connect: {
    assert(_server == e.peer);
    _connecting = false;
    on_connect();
    return;
  }
  case enet::Event_type::disconnect: {
    _server = {};
    _connecting = false;
    on_disconnect();
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
    case Message_type::player_join_response: {
      const auto player_network_id = deserialize<std::uint32_t>(reader);
      if (!player_network_id) {
        std::cerr << "Received malformed player_join_response message.\n";
        return;
      }
      on_player_join_response(*player_network_id);
      return;
    }
    case Message_type::game_state: {
      const auto world_state_size = deserialize<std::uint16_t>(reader);
      if (!world_state_size) {
        std::cerr << "Received malformed game_state message.\n";
        return;
      }
      const auto player_state_count = deserialize<std::uint8_t>(reader);
      if (!player_state_count) {
        std::cerr << "Received malformed game_state message.\n";
        return;
      }
      auto world_state_reader = serial::Span_reader{
          reader.data().subspan(reader.offset(), *world_state_size)};
      auto player_states_reader = serial::Span_reader{
          reader.data().subspan(reader.offset() + *world_state_size)};
      on_game_state(world_state_reader, player_states_reader,
                    *player_state_count);
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
