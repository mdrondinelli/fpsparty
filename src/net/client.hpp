#ifndef FPSPARTY_NET_CLIENT_H
#define FPSPARTY_NET_CLIENT_H

#include "enet.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include "net/core/sequence_number.hpp"
#include "serial/span_reader.hpp"

namespace fpsparty::net {
struct Client_create_info {
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
};

class Client {
public:
  explicit Client(Client_create_info const &create_info);

  virtual ~Client() = default;

  void poll_events();

  void wait_events(std::uint32_t timeout);

  void connect(enet::Address const &address);

  bool is_connecting() const noexcept;

  bool is_connected() const noexcept;

  void send_player_join_request();

  void send_player_leave_request(net::Entity_id player_entity_id);

  void send_player_input_state(
    net::Entity_id player_entity_id,
    net::Sequence_number input_sequence_number,
    net::Input_state const &input_state);

protected:
  virtual void on_connect();

  virtual void on_disconnect();

  virtual void on_player_join_response(net::Entity_id player_entity_id);

  virtual void on_world_snapshot(
    net::Sequence_number tick_number,
    serial::Span_reader &grid_state_reader,
    serial::Span_reader &public_entity_state_reader,
    serial::Span_reader &player_entity_state_reader);

private:
  void handle_event(enet::Event const &e);

  enet::Unique_host _host{};
  enet::Peer _server{};
  bool _connecting{};
};
} // namespace fpsparty::net

#endif
