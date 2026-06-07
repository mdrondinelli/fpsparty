#ifndef FPSPARTY_NET_SERVER_HPP
#define FPSPARTY_NET_SERVER_HPP

#include "enet.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include "net/core/player_join_request_id.hpp"
#include <cstdint>
#include <memory_resource>
#include <vector>

namespace fpsparty::net {
struct Server_create_info {
  std::uint16_t port;
  std::size_t max_clients;
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
};

class Server {
public:
  explicit Server(Server_create_info const &create_info);

  virtual ~Server() = default;

  void poll_events();

  void wait_events(std::uint32_t timeout);

  void disconnect();

  std::size_t get_peer_count() const noexcept;

  std::pmr::vector<enet::Peer> get_peers(
    std::pmr::memory_resource *memory_resource =
      std::pmr::get_default_resource()) const;

protected:
  virtual void on_peer_connect(enet::Peer) = 0;

  virtual void on_peer_disconnect(enet::Peer) = 0;

  virtual void on_player_join_request(
    enet::Peer, Player_join_request_id request_id) = 0;

  virtual void
  on_player_leave_request(enet::Peer peer, Entity_id player_entity_id) = 0;

  virtual void on_player_input_state(
    enet::Peer peer,
    Entity_id player_entity_id,
    net::Input_state const &input_state) = 0;

  void send_player_join_response(
    enet::Peer peer,
    Player_join_request_id request_id,
    Entity_id player_entity_id);

  void send_world_snapshot(
    enet::Peer peer,
    std::span<std::byte const> grid_state,
    std::span<std::byte const> public_entity_state,
    std::span<std::byte const> player_entity_state);

private:
  void handle_event(enet::Event const &e);

  enet::Unique_host _host;
  std::vector<enet::Peer> _peers;
};
} // namespace fpsparty::net

#endif
