#include "replicated_game.hpp"
#include "serial/serialize.hpp"
#include <vector>

namespace fpsparty::game {
struct Replicated_player::Impl {
  bool marked{};
  Eigen::Vector3f position{0.0f, 0.0f, 0.0f};
};

const Eigen::Vector3f &Replicated_player::get_position() const noexcept {
  return _impl->position;
}

struct Replicated_game::Impl {
  std::unordered_map<std::uint32_t, Replicated_player::Impl> players{};
};

void Replicated_game::apply_snapshot(serial::Reader &reader) const {
  using serial::deserialize;
  const auto player_count = deserialize<std::uint8_t>(reader);
  if (!player_count) {
    throw Updating_error{};
  }
  for (auto it = _impl->players.begin(); it != _impl->players.end(); ++it) {
    it->second.marked = false;
  }
  for (auto i = 0u; i != *player_count; ++i) {
    const auto player_id = deserialize<std::uint32_t>(reader);
    if (!player_id) {
      throw Updating_error{};
    }
    auto &player = _impl->players.try_emplace(*player_id).first->second;
    player.marked = true;
    const auto player_position_x = deserialize<float>(reader);
    if (!player_position_x) {
      throw Updating_error{};
    }
    const auto player_position_y = deserialize<float>(reader);
    if (!player_position_y) {
      throw Updating_error{};
    }
    const auto player_position_z = deserialize<float>(reader);
    if (!player_position_z) {
      throw Updating_error{};
    }
    player.position.x() = *player_position_x;
    player.position.y() = *player_position_y;
    player.position.z() = *player_position_z;
  }
  for (auto it = _impl->players.begin(); it != _impl->players.end();) {
    if (!it->second.marked) {
      it = _impl->players.erase(it);
    } else {
      ++it;
    }
  }
}

Replicated_player Replicated_game::get_player(std::uint32_t id) const noexcept {
  const auto it = _impl->players.find(id);
  return Replicated_player{it != _impl->players.end() ? &it->second : nullptr};
}

std::pmr::vector<Replicated_player>
Replicated_game::get_players(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Replicated_player>(memory_resource);
  retval.reserve(_impl->players.size());
  for (auto &[player_id, player] : _impl->players) {
    retval.emplace_back(Replicated_player{&player});
  }
  return retval;
}

Replicated_game create_replicated_game(const Replicated_game::Create_info &) {
  return Replicated_game{new Replicated_game::Impl};
}

void destroy_replicated_game(Replicated_game replicated_game) noexcept {
  delete replicated_game._impl;
}
}; // namespace fpsparty::game
