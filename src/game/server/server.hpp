#ifndef FPSPARTY_GAME_SERVER_HPP
#define FPSPARTY_GAME_SERVER_HPP

#include "game/server/game.hpp"
#include "net/server.hpp"

namespace fpsparty::game {
struct Server_create_info {
  std::uint16_t port;
  std::size_t max_clients;
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
  float tick_duration;
};

class Server : public net::Server {
public:
  explicit Server(const Server_create_info &info);

  bool service_game_state(float duration);

  void broadcast_game_state();

protected:
  void on_peer_connect(enet::Peer peer) override;

  void on_peer_disconnect(enet::Peer peer) override;

  void on_player_join_request(enet::Peer peer) override;

  void on_player_leave_request(enet::Peer peer,
                               net::Entity_id player_entity_id) override;

  void on_player_input_state(enet::Peer peer, net::Entity_id player_entity_id,
                             net::Sequence_number input_sequence_number,
                             const net::Input_state &input_state) override;

private:
  Game _game{{}};
  float _tick_duration{};
  float _tick_timer{};
};
} // namespace fpsparty::game

#endif
