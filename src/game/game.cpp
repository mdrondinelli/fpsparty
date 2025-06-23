#include "game.hpp"
#include "constants.hpp"
#include "game/humanoid_movement.hpp"
#include "game/projectile_movement.hpp"
#include "math/transformation_matrices.hpp"
#include "serial/serialize.hpp"
#include <Eigen/Dense>
#include <algorithm>

namespace fpsparty::game {
struct Game::Impl {
  std::vector<Player> players{};
  std::vector<Projectile> projectiles{};
  std::uint32_t next_network_id{};
};

Game create_game(const Game::Create_info &) { return Game{new Game::Impl}; }

void destroy_game(Game game) noexcept {
  game.clear();
  delete game._impl;
}

void Game::clear() const noexcept {
  for (const auto &player : _impl->players) {
    Player::delete_impl(player._impl);
  }
  for (const auto &projectile : _impl->projectiles) {
    Projectile::delete_impl(projectile._impl);
  }
  _impl->players.clear();
  _impl->projectiles.clear();
}

void Game::simulate(const Simulate_info &info) const {
  for (const auto &player : _impl->players) {
    player.decrease_attack_cooldown(info.duration);
    if (player.get_input_state().use_primary &&
        player.get_attack_cooldown() == 0.0f) {
      const auto player_look_direction =
          (math::y_rotation_matrix(player.get_input_state().yaw) *
           math::x_rotation_matrix(player.get_input_state().pitch))
              .col(2)
              .head<3>()
              .eval();
      create_projectile({
          .position = player.get_position(),
          .velocity = 10.0f * player_look_direction,
      });
      player.set_attack_cooldown(constants::attack_cooldown);
    }
    const auto movement_result = simulate_humanoid_movement({
        .initial_position = player.get_position(),
        .input_state = player.get_input_state(),
        .duration = info.duration,
    });
    player.set_position(movement_result.final_position);
  }
  for (const auto &projectile : _impl->projectiles) {
    const auto movement_result = simulate_projectile_movement({
        .initial_position = projectile.get_position(),
        .initial_velocity = projectile.get_velocity(),
        .duration = info.duration,
    });
    projectile.set_position(movement_result.final_position);
    projectile.set_velocity(movement_result.final_velocity);
  }
  for (const auto &projectile : get_projectiles()) {
    if (projectile.get_position().y() < -0.5f) {
      destroy_projectile(projectile);
    }
  }
}

void Game::snapshot(serial::Writer &writer) const {
  using serial::serialize;
  serialize<std::uint8_t>(writer, _impl->players.size());
  for (const auto &player : _impl->players) {
    serialize<std::uint32_t>(writer, player.get_network_id());
    serialize<float>(writer, player.get_position().x());
    serialize<float>(writer, player.get_position().y());
    serialize<float>(writer, player.get_position().z());
    serialize<Player::Input_state>(writer, player.get_input_state());
    serialize<std::optional<std::uint16_t>>(writer,
                                            player.get_input_sequence_number());
  }
  serialize<std::uint16_t>(writer, _impl->projectiles.size());
  for (const auto &projectile : _impl->projectiles) {
    serialize<std::uint32_t>(writer, projectile.get_network_id());
    serialize<float>(writer, projectile.get_position().x());
    serialize<float>(writer, projectile.get_position().y());
    serialize<float>(writer, projectile.get_position().z());
    serialize<float>(writer, projectile.get_velocity().x());
    serialize<float>(writer, projectile.get_velocity().y());
    serialize<float>(writer, projectile.get_velocity().z());
  }
}

Player Game::create_player(const Player::Create_info &) const {
  const auto retval = Player{Player::new_impl(_impl->next_network_id++)};
  _impl->players.emplace_back(retval);
  return retval;
}

void Game::destroy_player(Player player) const noexcept {
  const auto it = std::ranges::find(_impl->players, player);
  if (it != _impl->players.end()) {
    _impl->players.erase(it);
    Player::delete_impl(player._impl);
  }
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

Projectile Game::create_projectile(const Projectile::Create_info &info) const {
  const auto retval =
      Projectile{Projectile::new_impl(_impl->next_network_id++)};
  retval.set_position(info.position);
  retval.set_velocity(info.velocity);
  _impl->projectiles.emplace_back(retval);
  return retval;
}

void Game::destroy_projectile(Projectile projectile) const noexcept {
  const auto it = std::ranges::find(_impl->projectiles, projectile);
  if (it != _impl->projectiles.end()) {
    _impl->projectiles.erase(it);
    Projectile::delete_impl(projectile._impl);
  }
}

std::pmr::vector<Projectile>
Game::get_projectiles(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Projectile>{memory_resource};
  retval.reserve(_impl->projectiles.size());
  retval.insert_range(retval.begin(), _impl->projectiles);
  return retval;
}
} // namespace fpsparty::game
