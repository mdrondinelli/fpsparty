#ifndef FPSPARTY_CLIENT_CLIENT_HPP
#define FPSPARTY_CLIENT_CLIENT_HPP

#include <cstddef>

#include <net/client.hpp>
#include <net/entity_id.hpp>
#include <scene/scene.hpp>

#include "session.hpp"

namespace fpsparty::client {
struct Client_create_info {
  net::Client_create_info net_info;
  std::uint32_t max_buffered_ticks;
  float tick_duration;
};

class Client : public net::Client {
public:
  explicit Client(Client_create_info const &info);

  Client(Client const &other) = delete;

  Client &operator=(Client const &other) = delete;

  void update(float duration);

  std::optional<Session> const &get_session() const noexcept {
    return _session;
  }

  std::optional<Session> &get_session() noexcept { return _session; }

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
  std::uint32_t _max_buffered_ticks{};
  float _tick_duration{};
  std::optional<Session> _session{};
};
} // namespace fpsparty::client

#endif
