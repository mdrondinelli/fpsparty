#include "client.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "net/message_type.hpp"
#include "serial/ostream_writer.hpp"
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

void Client::send_player_input_state(
    const game::Player::Input_state &input_state,
    std::uint16_t sequence_number) {
  auto packet_writer = serial::Ostringstream_writer{};
  using serial::serialize;
  serialize<net::Message_type>(packet_writer,
                               net::Message_type::player_input_state);
  serialize<game::Player::Input_state>(packet_writer, input_state);
  serialize<std::uint16_t>(packet_writer, sequence_number);
  _server.send(constants::player_input_state_channel_id,
               enet::create_packet_unique(
                   {.data = packet_writer.stream().view().data(),
                    .data_length = packet_writer.stream().view().length()}));
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
    case Message_type::player_id: {
      const auto player_id = deserialize<std::uint32_t>(reader);
      if (!player_id) {
        std::cerr << "Received malformed player_id message.\n";
        return;
      }
      on_player_id(*player_id);
      return;
    }
    case Message_type::game_state:
      on_game_state(reader);
      return;
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
