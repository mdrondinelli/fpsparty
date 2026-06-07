#include "client.hpp"
#include "game/constants.hpp"
#include "game/entity_type.hpp"
#include "serial/serialize.hpp"

#include <Eigen/Geometry>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <tracy/Tracy.hpp>

namespace fpsparty::client {
namespace {
Eigen::Quaternionf yaw_orientation(float yaw) {
  return Eigen::Quaternionf{Eigen::AngleAxisf{yaw, Eigen::Vector3f::UnitY()}};
}
} // namespace

Client::Client(Client_create_info const &info)
    : net::Client{info.net_info}, _tick_duration{info.tick_duration} {}

void Client::tick(float duration) {
  if (!has_scene()) {
    return;
  }
  _tick_timer -= duration;
  if (_tick_timer <= 0) {
    auto const tracy_frame_name = "Input Tick";
    FrameMarkStart(tracy_frame_name);
    try {
      _tick_timer += _tick_duration;
      if (_player_entity_id) {
        send_player_input_state(*_player_entity_id, _current_input_state);
      }
    } catch (...) {
      FrameMarkEnd(tracy_frame_name);
      throw;
    }
    FrameMarkEnd(tracy_frame_name);
  }
}

bool Client::has_scene() const noexcept { return _scene.has_value(); }

net::Input_state const &Client::get_current_input_state() const noexcept {
  return _current_input_state;
}

void Client::set_current_input_state(net::Input_state const &value) noexcept {
  _current_input_state = value;
}

Scene *Client::get_scene() noexcept {
  return _scene ? &*_scene : nullptr;
}

Scene const *Client::get_scene() const noexcept {
  return _scene ? &*_scene : nullptr;
}

std::optional<Camera_snapshot> const &
Client::get_camera_snapshot() const noexcept {
  return _camera_snapshot;
}

void Client::on_connect() {
  _scene = Scene{};
  send_player_join_request();
}

void Client::on_disconnect() {
  _tick_timer = 0.0f;
  _last_grid_state_payload.clear();
  _player_entity_id = std::nullopt;
  _local_humanoid_entity_id = std::nullopt;
  _camera_snapshot = std::nullopt;
  _scene = std::nullopt;
}

void Client::on_player_join_response(net::Entity_id player_entity_id) {
  _player_entity_id = player_entity_id;
  std::cout << "Got player join response. id = " << player_entity_id << ".\n";
}

void Client::on_world_snapshot(
  serial::Span_reader &grid_state_reader,
  serial::Span_reader &public_entity_state_reader,
  serial::Span_reader &player_entity_state_reader) {
  ZoneScoped;
  assert(_scene);
  auto const grid_state = grid_state_reader.data();
  auto const grid_state_changed =
    grid_state.size() != _last_grid_state_payload.size() ||
    !std::ranges::equal(grid_state, _last_grid_state_payload);
  if (grid_state_changed) {
    _scene->get_grid().load(grid_state_reader);
    _scene->set_grid_remesh_flag();
    _last_grid_state_payload.assign(grid_state.begin(), grid_state.end());
  }
  load_player_entity_state(player_entity_state_reader);
  load_public_entity_state(public_entity_state_reader);
}

void Client::load_player_entity_state(serial::Reader &reader) {
  using serial::deserialize;
  _local_humanoid_entity_id = std::nullopt;
  auto const entity_count = deserialize<std::uint32_t>(reader);
  if (!entity_count) {
    std::cerr << "Failed to deserialize player entity count.\n";
    return;
  }
  for (auto i = std::uint32_t{}; i != *entity_count; ++i) {
    auto const entity_type = deserialize<game::Entity_type>(reader);
    auto const entity_id = deserialize<net::Entity_id>(reader);
    auto const humanoid_entity_id = deserialize<net::Entity_id>(reader);
    if (!entity_type || !entity_id || !humanoid_entity_id) {
      std::cerr << "Malformed player entity state.\n";
      return;
    }
    if (
      *entity_type == game::Entity_type::player && _player_entity_id &&
      *entity_id == *_player_entity_id && *humanoid_entity_id != 0) {
      _local_humanoid_entity_id = *humanoid_entity_id;
    }
  }
}

void Client::load_public_entity_state(serial::Reader &reader) {
  using serial::deserialize;
  auto &mesh_instances = _scene->get_mesh_instances();
  mesh_instances.clear();
  _camera_snapshot = std::nullopt;
  auto const entity_count = deserialize<std::uint32_t>(reader);
  if (!entity_count) {
    std::cerr << "Failed to deserialize public entity count.\n";
    return;
  }
  for (auto i = std::uint32_t{}; i != *entity_count; ++i) {
    auto const entity_type = deserialize<game::Entity_type>(reader);
    auto const entity_id = deserialize<net::Entity_id>(reader);
    if (!entity_type || !entity_id) {
      std::cerr << "Malformed public entity header.\n";
      return;
    }
    switch (*entity_type) {
    case game::Entity_type::humanoid: {
      auto const position = deserialize<Eigen::Vector3f>(reader);
      auto const input_state = deserialize<net::Input_state>(reader);
      if (!position || !input_state) {
        std::cerr << "Malformed humanoid state.\n";
        return;
      }
      if (
        _local_humanoid_entity_id &&
        *entity_id == *_local_humanoid_entity_id) {
        _camera_snapshot = Camera_snapshot{
          .position =
            *position + Eigen::Vector3f::UnitY() * game::constants::humanoid_eye_height,
          .yaw = input_state->yaw,
          .pitch = input_state->pitch,
        };
      } else {
        mesh_instances.emplace_back(Mesh_instance{
          .mesh = Mesh::cube,
          .position = *position + Eigen::Vector3f::UnitY() * 0.9f,
          .orientation = yaw_orientation(input_state->yaw),
          .scale = {0.7f, 1.8f, 0.7f},
        });
      }
      break;
    }
    case game::Entity_type::projectile: {
      auto const position = deserialize<Eigen::Vector3f>(reader);
      auto const velocity = deserialize<Eigen::Vector3f>(reader);
      if (!position || !velocity) {
        std::cerr << "Malformed projectile state.\n";
        return;
      }
      mesh_instances.emplace_back(Mesh_instance{
        .mesh = Mesh::cube,
        .position = *position,
        .orientation = Eigen::Quaternionf::Identity(),
        .scale = Eigen::Vector3f::Constant(0.25f),
      });
      break;
    }
    case game::Entity_type::player:
      std::cerr << "Unexpected player entity in public state.\n";
      return;
    }
  }
}
} // namespace fpsparty::client
