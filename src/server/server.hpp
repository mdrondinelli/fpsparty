#ifndef FPSPARTY_SERVER_SERVER_HPP
#define FPSPARTY_SERVER_SERVER_HPP

#include "game/game.hpp"
#include "net/sequence_number.hpp"
#include "net/server.hpp"

namespace fpsparty::server {
struct Server_create_info {
  net::Server_create_info net_info;
  game::Game_create_info game_info;
  float tick_duration;
};

class Server : public net::Server {
public:
  explicit Server(Server_create_info const &info);

  bool update(float duration);

  void broadcast_game_state();

  game::Game const &get_game() const noexcept;

  game::Game &get_game() noexcept;

  net::Sequence_number get_tick_number() const noexcept;

protected:
  void on_peer_connect(enet::Peer peer) override;

  void on_peer_disconnect(enet::Peer peer) override;

  void on_player_join_request(
    enet::Peer peer, net::Player_join_request_id request_id) override;

  void on_player_leave_request(
    enet::Peer peer, net::Entity_id player_entity_id) override;

  void on_player_input_state(
    enet::Peer peer,
    net::Entity_id player_entity_id,
    net::Input_state const &input_state) override;

private:
  game::Game _game;
  float _tick_duration{};
  float _tick_timer{};
  net::Sequence_number _tick_number{};
};
} // namespace fpsparty::server

#endif
