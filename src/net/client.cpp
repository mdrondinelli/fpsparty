#include "client.hpp"
#include "enet.hpp"
#include "net/core/constants.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/message_type.hpp"
#include "serial/ostream_writer.hpp"
#include "serial/serialize.hpp"
#include "serial/span_reader.hpp"
#include <iostream>

namespace fpsparty::net {
Client::Client(const Client_create_info &create_info)
    : _host{enet::make_client_host_unique(
        {.max_channels = constants::max_channels,
         .incoming_bandwidth = create_info.incoming_bandwidth,
         .outgoing_bandwidth = create_info.outgoing_bandwidth}
      )} {}

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
  serialize<net::Message_type>(
    packet_writer, net::Message_type::player_join_request
  );
  _server.send(
    constants::player_initialization_channel_id,
    enet::create_packet_unique({
      .data = packet_writer.stream().view().data(),
      .data_length = packet_writer.stream().view().length(),
      .flags = enet::Packet_flag_bits::reliable,
    })
  );
}

void Client::send_player_leave_request(net::Entity_id player_entity_id) {
  auto packet_writer = serial::Ostringstream_writer{};
  using serial::serialize;
  serialize<net::Message_type>(
    packet_writer, net::Message_type::player_leave_request
  );
  serialize<net::Entity_id>(packet_writer, player_entity_id);
  _server.send(
    constants::player_initialization_channel_id,
    enet::create_packet_unique({
      .data = packet_writer.stream().view().data(),
      .data_length = packet_writer.stream().view().length(),
      .flags = enet::Packet_flag_bits::reliable,
    })
  );
}

void Client::send_player_input_state(
  net::Entity_id player_entity_id,
  net::Sequence_number input_sequence_number,
  const net::Input_state &input_state
) {
  auto packet_writer = serial::Ostringstream_writer{};
  using serial::serialize;
  serialize<net::Message_type>(
    packet_writer, net::Message_type::player_input_state
  );
  serialize<net::Entity_id>(packet_writer, player_entity_id);
  serialize<net::Sequence_number>(packet_writer, input_sequence_number);
  serialize<net::Input_state>(packet_writer, input_state);
  _server.send(
    constants::player_input_state_channel_id,
    enet::create_packet_unique({
      .data = packet_writer.stream().view().data(),
      .data_length = packet_writer.stream().view().length(),
    })
  );
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
      const auto player_entity_id = deserialize<net::Entity_id>(reader);
      if (!player_entity_id) {
        goto malformed_message;
      }
      on_player_join_response(*player_entity_id);
      return;
    }
    case Message_type::grid_snapshot: {
      on_grid_snapshot(reader);
      return;
    }
    case Message_type::entity_snapshot: {
      const auto tick_number = deserialize<net::Sequence_number>(reader);
      if (!tick_number) {
        std::cerr << "Failed to deserialize tick_number.\n";
        goto malformed_message;
      }
      const auto public_state_size = deserialize<std::uint16_t>(reader);
      if (!public_state_size) {
        std::cerr << "Failed to deserialize public_state_size.\n";
        goto malformed_message;
      }
      auto public_state_reader =
        reader.subspan_reader(reader.offset(), *public_state_size);
      if (!public_state_reader) {
        std::cerr << "Failed to obtain public_state_reader.\n";
        goto malformed_message;
      }
      auto player_state_reader =
        reader.subspan_reader(reader.offset() + *public_state_size);
      if (!player_state_reader) {
        std::cerr << "Failed to obtain player_state_reader.\n";
        goto malformed_message;
      }
      on_entity_snapshot(
        *tick_number, *public_state_reader, *player_state_reader
      );
      return;
    }
    default:
      std::cerr << "Received packet with unexpected message type.\n";
      return;
    }
    return;
  malformed_message:
    std::cerr << "Received malformed " << magic_enum::enum_name(*message_type)
              << " message.\n";
    return;
  }
  default:
    return;
  }
}

void Client::on_connect() {}

void Client::on_disconnect() {}

void Client::on_player_join_response(net::Entity_id) {}

void Client::on_grid_snapshot(serial::Reader &) {}

void Client::
  on_entity_snapshot(net::Sequence_number, serial::Reader &, serial::Reader &) {
}
} // namespace fpsparty::net
