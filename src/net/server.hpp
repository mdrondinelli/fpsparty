#ifndef FPSPARTY_NET_SERVER_HPP
#define FPSPARTY_NET_SERVER_HPP

#include "enet.hpp"
#include "game/core/entity_id.hpp"
#include "game/core/humanoid_input_state.hpp"
#include "game/core/sequence_number.hpp"
#include <cstdint>
#include <memory_resource>

namespace fpsparty::net {
class Server {
public:
  struct Create_info {
    std::uint16_t port;
    std::size_t max_clients;
    std::uint32_t incoming_bandwidth{};
    std::uint32_t outgoing_bandwidth{};
  };

  explicit Server(const Create_info &create_info);

  virtual ~Server() = default;

  void poll_events();

  void wait_events(std::uint32_t timeout);

  void disconnect();

  void send_player_join_response(enet::Peer peer,
                                 game::Entity_id player_entity_id);

  void send_grid(enet::Peer peer, std::span<const std::byte> state);

  void send_snapshot(enet::Peer peer, game::Sequence_number tick_number,
                     std::span<const std::byte> public_state,
                     std::span<const std::byte> player_state,
                     std::size_t player_state_count);

  std::size_t get_peer_count() const noexcept;

  std::pmr::vector<enet::Peer>
  get_peers(std::pmr::memory_resource *memory_resource =
                std::pmr::get_default_resource()) const;

protected:
  virtual void on_peer_connect(enet::Peer);

  virtual void on_peer_disconnect(enet::Peer);

  virtual void on_player_join_request(enet::Peer);

  virtual void on_player_leave_request(enet::Peer peer,
                                       game::Entity_id player_entity_id);

  virtual void
  on_player_input_state(enet::Peer peer, game::Entity_id player_entity_id,
                        game::Sequence_number input_sequence_number,
                        const game::Humanoid_input_state &input_state);

private:
  void handle_event(const enet::Event &e);

  enet::Unique_host _host;
  std::vector<enet::Peer> _peers;
};
} // namespace fpsparty::net

#endif
