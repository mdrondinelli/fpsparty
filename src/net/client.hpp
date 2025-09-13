#ifndef FPSPARTY_NET_CLIENT_H
#define FPSPARTY_NET_CLIENT_H

#include "enet.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include "net/core/sequence_number.hpp"
#include "serial/reader.hpp"

namespace fpsparty::net {
struct Client_create_info {
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
};

class Client {
public:
  explicit Client(const Client_create_info &create_info);

  virtual ~Client() = default;

  void poll_events();

  void wait_events(std::uint32_t timeout);

  void connect(const enet::Address &address);

  bool is_connecting() const noexcept;

  bool is_connected() const noexcept;

  void send_player_join_request();

  void send_player_leave_request(net::Entity_id player_entity_id);

  void send_player_input_state(
    net::Entity_id player_entity_id,
    net::Sequence_number input_sequence_number,
    const net::Input_state &input_state
  );

protected:
  virtual void on_connect();

  virtual void on_disconnect();

  virtual void on_player_join_response(net::Entity_id player_entity_id);

  virtual void on_grid_snapshot(serial::Reader &state_reader);

  virtual void on_entity_snapshot(
    net::Sequence_number tick_number,
    serial::Reader &public_state_reader,
    serial::Reader &player_state_reader
  );

private:
  void handle_event(const enet::Event &e);

  enet::Unique_host _host{};
  enet::Peer _server{};
  bool _connecting{};
};
} // namespace fpsparty::net

#endif
