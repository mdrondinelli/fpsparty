#ifndef FPSPARTY_GAME_SERVER_HPP
#define FPSPARTY_GAME_SERVER_HPP

#include "game/game.hpp"
#include "net/server.hpp"

namespace fpsparty::game {
struct Server_create_info {
  net::Server_create_info net_info;
  Game_create_info game_info;
  float tick_duration;
};

class Server : public net::Server {
public:
  explicit Server(Server_create_info const &info);

  bool service_game_state(float duration);

  void broadcast_game_state();

  Game const &get_game() const noexcept;

  Game &get_game() noexcept;

protected:
  void on_peer_connect(enet::Peer peer) override;

  void on_peer_disconnect(enet::Peer peer) override;

  void on_player_join_request(enet::Peer peer) override;

  void on_player_leave_request(
    enet::Peer peer, net::Entity_id player_entity_id) override;

  void on_player_input_state(
    enet::Peer peer,
    net::Entity_id player_entity_id,
    net::Input_state const &input_state) override;

private:
  Game _game;
  float _tick_duration{};
  float _tick_timer{};
};
} // namespace fpsparty::game

#endif
