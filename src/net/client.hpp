#ifndef FPSPARTY_NET_CLIENT_H
#define FPSPARTY_NET_CLIENT_H

#include "enet.hpp"
#include "net/entity_id.hpp"
#include "net/input_state.hpp"
#include "net/player_join_request_id.hpp"
#include "net/sequence_number.hpp"
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

  void send_player_join_request(Player_join_request_id request_id);

  void send_player_leave_request(net::Entity_id player_entity_id);

  void send_player_input_state(
    net::Entity_id player_entity_id, net::Input_state const &input_state);

protected:
  virtual void on_connect() = 0;

  virtual void on_disconnect() = 0;

  virtual void on_player_join_response(
    Player_join_request_id request_id, net::Entity_id player_entity_id) = 0;

  virtual void on_world_snapshot(
    Sequence_number tick_number,
    serial::Span_reader &grid_state_reader,
    serial::Span_reader &public_entity_state_reader,
    serial::Span_reader &player_entity_state_reader) = 0;

private:
  void handle_event(enet::Event const &e);

  enet::Unique_host _host{};
  enet::Peer _server{};
  bool _connecting{};
};
} // namespace fpsparty::net

#endif
