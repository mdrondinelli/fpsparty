#ifndef FPSPARTY_GAME_CLIENT_CLIENT_HPP
#define FPSPARTY_GAME_CLIENT_CLIENT_HPP

#include "game/client/replicated_game.hpp"
#include "net/client.hpp"
#include "net/core/entity_id.hpp"
#include <optional>

namespace fpsparty::game {
struct Client_create_info {
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
  float tick_duration;
};

class Client : net::Client {
public:
  explicit Client(const Client_create_info &info);

  void service_game_state(float duration);

  bool has_game_state() const noexcept;

  void poll_events();

  void wait_events(std::uint32_t timeout);

  void connect(const enet::Address &address);

  bool is_connecting() const noexcept;

  bool is_connected() const noexcept;

protected:
  void on_connect() override;

  void on_disconnect() override;

  void on_player_join_response(net::Entity_id player_entity_id) override;

  void on_grid_snapshot(serial::Reader &reader) override;

  void on_entity_snapshot(net::Sequence_number tick_number,
                          serial::Reader &public_state_reader,
                          serial::Reader &player_state_reader) override;

  Replicated_game *get_game() noexcept;

  const Replicated_game *get_game() const noexcept;

  Replicated_player *get_player() const noexcept;

private:
  std::optional<Replicated_game> _game{};
  std::optional<net::Entity_id> _player_entity_id{};
  net::Sequence_number _input_sequence_number{};
  std::vector<std::pair<net::Input_state, net::Sequence_number>>
      _in_flight_input_states{};
  float _tick_duration{};
  float _tick_timer{};
};
} // namespace fpsparty::game

#endif
