#include "session.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

#include <tracy/Tracy.hpp>

#include <game/constants.hpp>
#include <game/entity_type.hpp>

namespace fpsparty::client {

namespace {

Eigen::Quaternionf yaw_orientation(float yaw) {
  return Eigen::Quaternionf{Eigen::AngleAxisf{yaw, Eigen::Vector3f::UnitY()}};
}

} // namespace

Session::Session(Session_create_info const &info)
    : _client{info.client},
      _scene{{
        .max_buffered_keyframes = info.max_buffered_ticks,
        .keyframe_duration = info.tick_duration,
      }} {}

void Session::update(float duration) {
  _tick_timer -= duration;
  if (_tick_timer <= 0) {
    _tick_timer += _scene.get_keyframe_duration();
    for (auto const &player : _local_players) {
      if (player->state == Local_player_state::joined) {
        _client->send_player_input_state(
          *player->player_entity_id, player->input_state);
      }
    }
  }
  if (_state == State::playing) {
    if (!_scene.update(duration) && _scene.get_max_buffered_keyframes() > 1) {
      std::cerr << "Warning: server is lagging behind.\n";
      _state = State::buffering;
    }
  }
}

Local_player &Session::join_player() {
  assert(_next_player_join_request_id != 0);
  auto &retval = *_local_players.emplace_back(
    std::make_unique<Local_player>(_next_player_join_request_id));
  try {
    _client->send_player_join_request(_next_player_join_request_id);
  } catch (...) {
    _local_players.pop_back();
    throw;
  }
  ++_next_player_join_request_id;
  return retval;
}

void Session::leave_player(Local_player &player) {
  auto const it = std::ranges::find(
    _local_players, &player, [](auto const &value) { return value.get(); });
  assert(it != _local_players.end());
  // need to be joined to have a player entity id to send a leave request
  // TODO: leave request should take the same id as the join request, so the
  // client can cancel before waiting for the join to succeed.
  assert(player.state == Local_player_state::joined);
  _client->send_player_leave_request(*player.player_entity_id);
  _local_players.erase(it);
}

void Session::on_player_join_response(
  net::Player_join_request_id request_id, net::Entity_id player_entity_id) {
  auto const it = std::ranges::find_if(_local_players, [&](auto const &player) {
    return player->state == Local_player_state::joining &&
           player->request_id == request_id;
  });
  if (it == _local_players.end()) {
    std::cerr << "Got unexpected player join response. request id = "
              << request_id << ".\n";
    return;
  }
  auto &player = **it;
  player.state = Local_player_state::joined;
  player.player_entity_id = player_entity_id;
  std::cout << "Got player join response. request id = " << request_id
            << ", entity id = " << player_entity_id << ".\n";
}

void Session::on_world_snapshot(
  net::Sequence_number tick_number,
  serial::Span_reader &grid_state_reader,
  serial::Span_reader &public_entity_state_reader,
  serial::Span_reader &player_entity_state_reader) {
  ZoneScoped;
  auto keyframe = scene::Keyframe{
    .number = tick_number,
    .grid = game::Grid{{}},
    .cameras = {},
    .mesh_instances = {},
  };
  keyframe.grid.load(grid_state_reader);
  load_player_state(player_entity_state_reader);
  load_public_state(public_entity_state_reader, keyframe);
  _scene.push_keyframe(std::move(keyframe));
  if (
    _state == State::buffering &&
    _scene.get_keyframe_count() == _scene.get_max_buffered_keyframes()) {
    _state = State::playing;
  }
}

void Session::load_player_state(serial::Reader &reader) {
  using serial::deserialize;
  for (auto const &player : _local_players) {
    player->humanoid_entity_id = std::nullopt;
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
        return player->state == Local_player_state::joined &&
               player->player_entity_id == *entity_id;
      });
    if (player_it != _local_players.end() && *humanoid_entity_id != 0) {
      (*player_it)->humanoid_entity_id = *humanoid_entity_id;
    }
  }
}

void Session::load_public_state(
  serial::Reader &reader, scene::Keyframe &keyframe) {
  using serial::deserialize;
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
          return player->humanoid_entity_id &&
                 *player->humanoid_entity_id == *entity_id;
        });
      if (player_it != _local_players.end()) {
        keyframe.cameras.emplace_back(
          *(*player_it)->player_entity_id,
          scene::Camera{
            .position = *position + Eigen::Vector3f::UnitY() *
                                      game::constants::humanoid_eye_height,
            .yaw = input_state->yaw,
            .pitch = input_state->pitch,
          });
      } else {
        keyframe.mesh_instances.emplace_back(
          *entity_id,
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
      keyframe.mesh_instances.emplace_back(
        *entity_id,
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

scene::Scene &Session::get_scene() noexcept { return _scene; }

scene::Scene const &Session::get_scene() const noexcept { return _scene; }

} // namespace fpsparty::client
