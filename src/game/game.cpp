#include "game.hpp"
#include "constants.hpp"
#include "serial/serialize.hpp"
#include <Eigen/Dense>
#include <algorithm>

namespace fpsparty::game {
struct Game::Impl {
  std::vector<Player> players{};
  std::uint32_t next_player_id{};
};

struct Player::Impl {
  std::uint32_t id;
  Eigen::Vector3f position{0.0f, 0.0f, 0.0f};
  Input_state input_state{};
};

std::uint32_t Player::get_id() const noexcept { return _impl->id; }

Player::Input_state Player::get_input_state() const noexcept {
  return _impl->input_state;
}

void Player::set_input_state(const Input_state &input_state) const noexcept {
  _impl->input_state = input_state;
}

Game create_game(const Game::Create_info &) { return Game{new Game::Impl}; }

void destroy_game(Game game) noexcept { delete game._impl; }

Player Game::create_player(const Player::Create_info &) const {
  const auto retval = Player{new Player::Impl{.id = _impl->next_player_id++}};
  _impl->players.emplace_back(retval);
  return retval;
}

void Game::destroy_player(Player player) const noexcept {
  const auto it = std::ranges::find(_impl->players, player);
  if (it != _impl->players.end()) {
    _impl->players.erase(it);
    delete player._impl;
  }
}

Unique_player
Game::create_player_unique(const Player::Create_info &info) const {
  return Unique_player{create_player(info), *this};
}

std::size_t Game::get_player_count() const noexcept {
  return _impl->players.size();
}

void Game::simulate(const Simulate_info &info) const {
  for (const auto &player : _impl->players) {
    const auto input_state = player.get_input_state();
    auto movement_vector = Eigen::Vector3f{0.0f, 0.0f, 0.0f};
    if (input_state.move_left) {
      movement_vector += Eigen::Vector3f{1.0f, 0.0f, 0.0f};
    }
    if (input_state.move_right) {
      movement_vector -= Eigen::Vector3f{1.0f, 0.0f, 0.0f};
    }
    if (input_state.move_forward) {
      movement_vector += Eigen::Vector3f{0.0f, 0.0f, 1.0f};
    }
    if (input_state.move_backward) {
      movement_vector -= Eigen::Vector3f{0.0f, 0.0f, 1.0f};
    }
    movement_vector.normalize();
    player._impl->position +=
        movement_vector * constants::player_movement_speed * info.duration;
  }
}

void Game::snapshot(serial::Writer &writer) const {
  using serial::serialize;
  serialize<std::uint8_t>(writer, _impl->players.size());
  for (const auto &player : _impl->players) {
    serialize<std::uint32_t>(writer, player._impl->id);
    serialize<float>(writer, player._impl->position.x());
    serialize<float>(writer, player._impl->position.y());
    serialize<float>(writer, player._impl->position.z());
  }
}
} // namespace fpsparty::game
