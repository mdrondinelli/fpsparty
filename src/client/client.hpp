#ifndef FPSPARTY_CLIENT_CLIENT_HPP
#define FPSPARTY_CLIENT_CLIENT_HPP

#include "client/local_player.hpp"
#include "net/client.hpp"
#include "net/entity_id.hpp"
#include "net/input_state.hpp"
#include "scene/scene.hpp"
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace fpsparty::client {
struct Client_create_info {
  net::Client_create_info net_info;
  float tick_duration;
};

class Client : public net::Client {
public:
  explicit Client(Client_create_info const &info);

  Client(Client const &other) = delete;

  Client &operator=(Client const &other) = delete;

  void tick(float duration);

  bool has_scene() const noexcept;

  Local_player &join_player();

  void leave_player(Local_player &player);

  scene::Scene *get_scene() noexcept;

  scene::Scene const *get_scene() const noexcept;

protected:
  void on_connect() override;

  void on_disconnect() override;

  void on_player_join_response(
    net::Player_join_request_id request_id,
    net::Entity_id player_entity_id) override;

  void on_world_snapshot(
    net::Sequence_number tick_number,
    serial::Span_reader &grid_state_reader,
    serial::Span_reader &public_entity_state_reader,
    serial::Span_reader &player_entity_state_reader) override;

private:
  void load_player_entity_state(serial::Reader &reader);

  void load_public_entity_state(serial::Reader &reader);

  std::optional<scene::Scene> _scene{};
  std::vector<std::unique_ptr<Local_player>> _local_players{};
  std::vector<std::byte> _last_grid_state_payload{};
  net::Player_join_request_id _next_player_join_request_id{1};
  float _tick_duration{};
  float _tick_timer{};
};
} // namespace fpsparty::client

#endif
