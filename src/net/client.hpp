#ifndef FPSPARTY_NET_CLIENT_H
#define FPSPARTY_NET_CLIENT_H

#include "enet.hpp"
#include "game/core/humanoid_input_state.hpp"
#include "serial/reader.hpp"

namespace fpsparty::net {
class Client {
public:
  struct Create_info {
    std::uint32_t incoming_bandwidth{};
    std::uint32_t outgoing_bandwidth{};
  };

  explicit Client(const Create_info &create_info);

  virtual ~Client() = default;

  void poll_events();

  void wait_events(std::uint32_t timeout);

  void connect(const enet::Address &address);

  bool is_connecting() const noexcept;

  bool is_connected() const noexcept;

  void send_player_join_request();

  void send_player_leave_request(std::uint32_t player_network_id);

  void send_player_input_state(std::uint32_t player_network_id,
                               std::uint16_t input_sequence_number,
                               const game::Humanoid_input_state &input_state);

protected:
  virtual void on_connect() {}

  virtual void on_disconnect() {}

  virtual void on_player_join_response(std::uint32_t /*player_network_id*/) {}

  virtual void on_game_state(serial::Reader & /*world_state_reader*/,
                             serial::Reader & /*player_states_reader*/,
                             std::uint8_t /*player_state_count*/) {}

private:
  void handle_event(const enet::Event &e);

  enet::Unique_host _host{};
  enet::Peer _server{};
  bool _connecting{};
};
} // namespace fpsparty::net

#endif
