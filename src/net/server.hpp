#ifndef FPSPARTY_NET_SERVER_HPP
#define FPSPARTY_NET_SERVER_HPP

#include "enet.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include "net/core/sequence_number.hpp"
#include <cstdint>
#include <memory_resource>

namespace fpsparty::net {
struct Server_create_info {
  std::uint16_t port;
  std::size_t max_clients;
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
};

class Server {
public:
  explicit Server(const Server_create_info &create_info);

  virtual ~Server() = default;

  void poll_events();

  void wait_events(std::uint32_t timeout);

  void disconnect();

  std::size_t get_peer_count() const noexcept;

  std::pmr::vector<enet::Peer>
  get_peers(std::pmr::memory_resource *memory_resource =
                std::pmr::get_default_resource()) const;

protected:
  virtual void on_peer_connect(enet::Peer);

  virtual void on_peer_disconnect(enet::Peer);

  virtual void on_player_join_request(enet::Peer);

  virtual void on_player_leave_request(enet::Peer peer,
                                       Entity_id player_entity_id);

  virtual void on_player_input_state(enet::Peer peer,
                                     Entity_id player_entity_id,
                                     Sequence_number input_sequence_number,
                                     const net::Input_state &input_state);

  void send_player_join_response(enet::Peer peer, Entity_id player_entity_id);

  void send_grid_snapshot(enet::Peer peer, std::span<const std::byte> state);

  void send_entity_snapshot(enet::Peer peer, Sequence_number tick_number,
                            std::span<const std::byte> public_state,
                            std::span<const std::byte> player_state);

private:
  void handle_event(const enet::Event &e);

  enet::Unique_host _host;
  std::vector<enet::Peer> _peers;
};
} // namespace fpsparty::net

#endif
