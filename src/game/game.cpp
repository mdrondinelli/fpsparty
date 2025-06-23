#include "game.hpp"
#include "constants.hpp"
#include "game/humanoid_movement.hpp"
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
  std::optional<std::uint16_t> input_sequence_number{};
  bool input_fresh{};
};

std::uint32_t Player::get_id() const noexcept { return _impl->id; }

Player::Input_state Player::get_input_state() const noexcept {
  return _impl->input_state;
}

void Player::set_input_state(const Input_state &input_state,
                             std::uint16_t input_sequence_number,
                             bool input_fresh) const noexcept {
  if (_impl->input_sequence_number) {
    const auto difference = static_cast<std::int16_t>(
        input_sequence_number - *_impl->input_sequence_number);
    if (difference < 0) {
      return;
    }
  }
  _impl->input_state = input_state;
  _impl->input_sequence_number = input_sequence_number;
  _impl->input_fresh = input_fresh;
}

std::optional<std::uint16_t>
Player::get_input_sequence_number() const noexcept {
  return _impl->input_sequence_number;
}

void Player::increment_input_sequence_number() const noexcept {
  if (_impl->input_sequence_number) {
    ++*_impl->input_sequence_number;
  }
}

bool Player::is_input_stale() const noexcept { return !_impl->input_fresh; }

void Player::mark_input_stale() const noexcept { _impl->input_fresh = false; }

const Eigen::Vector3f &Player::get_position() const noexcept {
  return _impl->position;
}

void Player::set_position(const Eigen::Vector3f &position) const noexcept {
  _impl->position = position;
}

Game create_game(const Game::Create_info &) { return Game{new Game::Impl}; }

void destroy_game(Game game) noexcept { delete game._impl; }

void Game::simulate(const Simulate_info &info) const {
  for (const auto &player : _impl->players) {
    const auto movement_result = simulate_humanoid_movement({
        .initial_position = player.get_position(),
        .input_state = player.get_input_state(),
        .duration = info.duration,
    });
    player.set_position(movement_result.final_position);
  }
}

void Game::snapshot(serial::Writer &writer) const {
  using serial::serialize;
  serialize<std::uint8_t>(writer, _impl->players.size());
  for (const auto &player : _impl->players) {
    serialize<std::uint32_t>(writer, player.get_id());
    serialize<float>(writer, player.get_position().x());
    serialize<float>(writer, player.get_position().y());
    serialize<float>(writer, player.get_position().z());
    serialize<Player::Input_state>(writer, player.get_input_state());
    serialize<std::optional<std::uint16_t>>(writer,
                                            player.get_input_sequence_number());
  }
}

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

std::pmr::vector<Player>
Game::get_players(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Player>{memory_resource};
  retval.reserve(_impl->players.size());
  retval.insert_range(retval.begin(), _impl->players);
  return retval;
}
} // namespace fpsparty::game
