#include "client.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

#include <Eigen/Geometry>
#include <tracy/Tracy.hpp>

#include <game/constants.hpp>
#include <game/entity_type.hpp>
#include <serial/serialize.hpp>

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
      for (auto const &player : _local_players) {
        if (player->_state == Local_player_state::joined) {
          send_player_input_state(
            *player->_player_entity_id, player->_input_state);
        }
      }
    } catch (...) {
      FrameMarkEnd(tracy_frame_name);
      throw;
    }
    FrameMarkEnd(tracy_frame_name);
  }
}

bool Client::has_scene() const noexcept { return _scene.has_value(); }

Local_player &Client::join_player() {
  assert(is_connected());
  assert(_next_player_join_request_id != 0);
  auto &retval = *_local_players.emplace_back(
    std::unique_ptr<Local_player>{
      new Local_player{_next_player_join_request_id}});
  try {
    send_player_join_request(_next_player_join_request_id);
  } catch (...) {
    _local_players.pop_back();
    throw;
  }
  ++_next_player_join_request_id;
  return retval;
}

void Client::leave_player(Local_player &player) {
  auto const it = std::ranges::find(
    _local_players, &player, [](auto const &value) { return value.get(); });
  assert(it != _local_players.end());
  assert(player._state == Local_player_state::joined);
  send_player_leave_request(*player._player_entity_id);
  _local_players.erase(it);
}

scene::Scene *Client::get_scene() noexcept {
  return _scene ? &*_scene : nullptr;
}

scene::Scene const *Client::get_scene() const noexcept {
  return _scene ? &*_scene : nullptr;
}

void Client::on_connect() { _scene = scene::Scene{}; }

void Client::on_disconnect() {
  _tick_timer = 0.0f;
  _last_grid_state_payload.clear();
  _local_players.clear();
  _next_player_join_request_id = 1;
  _scene = std::nullopt;
}

void Client::on_player_join_response(
  net::Player_join_request_id request_id, net::Entity_id player_entity_id) {
  auto const it = std::ranges::find_if(_local_players, [&](auto const &player) {
    return player->_state == Local_player_state::joining &&
           player->_request_id == request_id;
  });
  if (it == _local_players.end()) {
    std::cerr << "Got unexpected player join response. request id = "
              << request_id << ".\n";
    return;
  }
  auto &player = **it;
  player._state = Local_player_state::joined;
  player._player_entity_id = player_entity_id;
  std::cout << "Got player join response. request id = " << request_id
            << ", entity id = " << player_entity_id << ".\n";
}

void Client::on_world_snapshot(
  net::Sequence_number,
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
  for (auto const &player : _local_players) {
    player->_humanoid_entity_id = std::nullopt;
  }
  auto const entity_count = deserialize<std::uint32_t>(reader);
  if (!entity_count) {
    std::cerr << "Failed to deserialize player entity count.\n";
    return;
  }
  for (auto i = std::uint32_t{}; i != *entity_count; ++i) {
    auto const entity_type = deserialize<game::Entity_type>(reader);
    if (*entity_type != game::Entity_type::player) {
      std::cerr << "Malformed player entity state.\n";
      return;
    }
    auto const entity_id = deserialize<net::Entity_id>(reader);
    auto const humanoid_entity_id = deserialize<net::Entity_id>(reader);
    if (!entity_type || !entity_id || !humanoid_entity_id) {
      std::cerr << "Malformed player entity state.\n";
      return;
    }
    auto const player_it =
      std::ranges::find_if(_local_players, [&](auto const &player) {
        return player->_state == Local_player_state::joined &&
               player->_player_entity_id == *entity_id;
      });
    if (player_it != _local_players.end() && *humanoid_entity_id != 0) {
      (*player_it)->_humanoid_entity_id = *humanoid_entity_id;
    }
  }
}

void Client::load_public_entity_state(serial::Reader &reader) {
  using serial::deserialize;
  auto &mesh_instances = _scene->get_mesh_instances();
  mesh_instances.clear();
  for (auto const &player : _local_players) {
    player->_camera_snapshot = std::nullopt;
  }
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
      auto const player_it =
        std::ranges::find_if(_local_players, [&](auto const &player) {
          return player->_humanoid_entity_id &&
                 *player->_humanoid_entity_id == *entity_id;
        });
      if (player_it != _local_players.end()) {
        (*player_it)->_camera_snapshot = Camera_snapshot{
          .position = *position + Eigen::Vector3f::UnitY() *
                                    game::constants::humanoid_eye_height,
          .yaw = input_state->yaw,
          .pitch = input_state->pitch,
        };
      } else {
        mesh_instances.emplace_back(
          scene::Mesh_instance{
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
      mesh_instances.emplace_back(
        scene::Mesh_instance{
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
