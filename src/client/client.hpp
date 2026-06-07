#ifndef FPSPARTY_CLIENT_CLIENT_HPP
#define FPSPARTY_CLIENT_CLIENT_HPP

#include "net/client.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/input_state.hpp"
#include "scene/scene.hpp"
#include <Eigen/Dense>
#include <cstddef>
#include <optional>
#include <vector>

namespace fpsparty::client {
struct Client_create_info {
  net::Client_create_info net_info;
  float tick_duration;
};

struct Camera_snapshot {
  Eigen::Vector3f position{};
  float yaw{};
  float pitch{};
};

class Client : public net::Client {
public:
  explicit Client(Client_create_info const &info);

  Client(Client const &other) = delete;

  Client &operator=(Client const &other) = delete;

  void tick(float duration);

  bool has_scene() const noexcept;

  net::Input_state const &get_current_input_state() const noexcept;

  void set_current_input_state(net::Input_state const &value) noexcept;

  scene::Scene *get_scene() noexcept;

  scene::Scene const *get_scene() const noexcept;

  std::optional<Camera_snapshot> const &get_camera_snapshot() const noexcept;

protected:
  void on_connect() override;

  void on_disconnect() override;

  void on_player_join_response(net::Entity_id player_entity_id) override;

  void on_world_snapshot(
    serial::Span_reader &grid_state_reader,
    serial::Span_reader &public_entity_state_reader,
    serial::Span_reader &player_entity_state_reader) override;

private:
  void load_player_entity_state(serial::Reader &reader);

  void load_public_entity_state(serial::Reader &reader);

  std::optional<scene::Scene> _scene{};
  std::optional<net::Entity_id> _player_entity_id{};
  std::optional<net::Entity_id> _local_humanoid_entity_id{};
  std::optional<Camera_snapshot> _camera_snapshot{};
  std::vector<std::byte> _last_grid_state_payload{};
  net::Input_state _current_input_state{};
  float _tick_duration{};
  float _tick_timer{};
};
} // namespace fpsparty::client

#endif
