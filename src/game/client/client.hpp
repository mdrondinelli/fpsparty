#ifndef FPSPARTY_GAME_CLIENT_CLIENT_HPP
#define FPSPARTY_GAME_CLIENT_CLIENT_HPP

#include "game/client/replicated_game.hpp"
#include "net/client.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include <cstddef>
#include <optional>
#include <vector>

namespace fpsparty::game {
struct Client_create_info {
  std::uint32_t incoming_bandwidth{};
  std::uint32_t outgoing_bandwidth{};
  float tick_duration;
};

class Client : net::Client {
public:
  explicit Client(Client_create_info const &info);

  Client(Client const &other) = delete;

  Client &operator=(Client const &other) = delete;

  void service_game_state(float duration);

  bool has_game_state() const noexcept;

  void poll_events();

  void wait_events(std::uint32_t timeout);

  void connect(enet::Address const &address);

  bool is_connecting() const noexcept;

  bool is_connected() const noexcept;

  net::Input_state const &get_current_input_state() const noexcept;

  void set_current_input_state(net::Input_state const &value) noexcept;

protected:
  void on_connect() override;

  void on_disconnect() override;

  void on_player_join_response(net::Entity_id player_entity_id) override;

  virtual void on_update_grid();

  void on_world_snapshot(
    net::Sequence_number tick_number,
    serial::Span_reader &grid_state_reader,
    serial::Span_reader &public_entity_state_reader,
    serial::Span_reader &player_entity_state_reader) override;

  Replicated_game *get_game() noexcept;

  Replicated_game const *get_game() const noexcept;

  Replicated_player *get_player() const noexcept;

private:
  std::optional<Replicated_game> _game{};
  std::optional<net::Entity_id> _player_entity_id{};
  net::Sequence_number _input_sequence_number{};
  std::vector<std::pair<net::Input_state, net::Sequence_number>>
    _in_flight_input_states{};
  std::vector<std::byte> _last_grid_state_payload{};
  net::Input_state _current_input_state{};
  float _tick_duration{};
  float _tick_timer{};
};
} // namespace fpsparty::game

#endif
