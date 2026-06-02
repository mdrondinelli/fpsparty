#include "session.hpp"
#include "game/constants.hpp"
#include "game/entity_type.hpp"
#include "net/client.hpp"
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

Session::Session(Session_create_info const &info)
    : net::Client{{
        .incoming_bandwidth = info.incoming_bandwidth,
        .outgoing_bandwidth = info.outgoing_bandwidth,
      }},
      _tick_duration{info.tick_duration} {}

void Session::service_input_tick(float duration) {
  assert(has_scene());
  _tick_timer -= duration;
  if (_tick_timer <= 0) {
    auto const tracy_frame_name = "Input Tick";
    FrameMarkStart(tracy_frame_name);
    try {
      _tick_timer += _tick_duration;
      if (_player_entity_id) {
        net::Client::send_player_input_state(
          *_player_entity_id, _current_input_state);
      }
    } catch (...) {
      FrameMarkEnd(tracy_frame_name);
      throw;
    }
    FrameMarkEnd(tracy_frame_name);
  }
}

bool Session::has_scene() const noexcept { return _scene.has_value(); }

void Session::poll_events() {
  ZoneScoped;
  net::Client::poll_events();
}

void Session::wait_events(std::uint32_t timeout) {
  net::Client::wait_events(timeout);
}

void Session::connect(enet::Address const &address) {
  net::Client::connect(address);
}

bool Session::is_connecting() const noexcept {
  return net::Client::is_connecting();
}

bool Session::is_connected() const noexcept {
  return net::Client::is_connected();
}

net::Input_state const &Session::get_current_input_state() const noexcept {
  return _current_input_state;
}

void Session::set_current_input_state(net::Input_state const &value) noexcept {
  _current_input_state = value;
}

scene::Scene *Session::get_scene() noexcept {
  return _scene ? &*_scene : nullptr;
}

scene::Scene const *Session::get_scene() const noexcept {
  return _scene ? &*_scene : nullptr;
}

std::optional<Camera_snapshot> const &
Session::get_camera_snapshot() const noexcept {
  return _camera_snapshot;
}

void Session::on_connect() {
  _scene = scene::Scene{};
  send_player_join_request();
}

void Session::on_disconnect() {
  _tick_timer = 0.0f;
  _last_grid_state_payload.clear();
  _player_entity_id = std::nullopt;
  _local_humanoid_entity_id = std::nullopt;
  _camera_snapshot = std::nullopt;
  _scene = std::nullopt;
}

void Session::on_player_join_response(net::Entity_id player_entity_id) {
  _player_entity_id = player_entity_id;
  std::cout << "Got player join response. id = " << player_entity_id << ".\n";
}

void Session::on_update_grid() {}

void Session::on_world_snapshot(
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
    _last_grid_state_payload.assign(grid_state.begin(), grid_state.end());
    on_update_grid();
  }
  load_player_entity_state(player_entity_state_reader);
  load_public_entity_state(public_entity_state_reader);
}

void Session::load_player_entity_state(serial::Reader &reader) {
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

void Session::load_public_entity_state(serial::Reader &reader) {
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
        mesh_instances.emplace_back(scene::Mesh_instance{
          .mesh = scene::Mesh::cube,
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
      mesh_instances.emplace_back(scene::Mesh_instance{
        .mesh = scene::Mesh::cube,
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
