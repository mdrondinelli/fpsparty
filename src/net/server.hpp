#ifndef FPSPARTY_NET_SERVER_HPP
#define FPSPARTY_NET_SERVER_HPP

#include "enet.hpp"
#include "game/game.hpp"
#include <cstdint>

namespace fpsparty::net {
struct Server_create_info {
  std::uint16_t port;
  std::size_t max_clients;
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
  game::Game game;
};

class Server {
public:
  explicit Server(const Server_create_info &create_info);

  virtual ~Server() = default;

  void poll_events();

  void disconnect();

  void broadcast_game_state();

protected:
  game::Game get_game() const noexcept { return _game; }

  virtual void on_peer_connect(enet::Peer) {}

  virtual void on_peer_disconnect(enet::Peer) {}

  virtual void on_player_input_state(enet::Peer,
                                     const game::Player::Input_state &) {}

private:
  enet::Unique_host _host;
  std::vector<enet::Peer> _peers;
  game::Game _game;
};
} // namespace fpsparty::net

#endif
