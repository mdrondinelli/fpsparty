#ifndef FPSPARTY_NET_SERVER_HPP
#define FPSPARTY_NET_SERVER_HPP

#include "enet.hpp"
#include "game/game.hpp"
#include <cstdint>

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

  void send_player_id(enet::Peer peer, std::uint32_t player_id);

  void broadcast_game_state(game::Game game);

protected:
  virtual void on_peer_connect(enet::Peer) {}

  virtual void on_peer_disconnect(enet::Peer) {}

  virtual void on_player_input_state(enet::Peer,
                                     const game::Player::Input_state &) {}

private:
  void handle_event(const enet::Event &e);

  enet::Unique_host _host;
  std::vector<enet::Peer> _peers;
};
} // namespace fpsparty::net

#endif
