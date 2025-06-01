#include "game.hpp"
#include "constants.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
struct Game::Impl {};

struct Player::Impl {
  Eigen::Vector2f position;
};

Game create_game(const Game::Create_info &) { return Game{new Game::Impl}; }

void destroy_game(Game game) noexcept { delete game._impl; }

Player Game::create_player(const Player::Create_info &) const {
  return Player{new Player::Impl};
}

void Game::destroy_player(Player player) const noexcept { delete player._impl; }

Unique_player
Game::create_player_unique(const Player::Create_info &info) const {
  return Unique_player{create_player(info), *this};
}

void Game::simulate(const Simulate_info &info) const {
  for (const auto &player_input : info.player_inputs) {
    auto movement_vector = Eigen::Vector2f{0.0f, 0.0f};
    if (player_input.move_left) {
      movement_vector -= Eigen::Vector2f{1.0f, 0.0f};
    }
    if (player_input.move_right) {
      movement_vector += Eigen::Vector2f{1.0f, 0.0f};
    }
    if (player_input.move_forward) {
      movement_vector += Eigen::Vector2f{0.0f, 1.0f};
    }
    if (player_input.move_backward) {
      movement_vector -= Eigen::Vector2f{1.0f, 0.0f};
    }
    movement_vector.normalize();
    player_input.player._impl->position +=
        movement_vector * constants::player_movement_speed * info.duration;
  }
}
} // namespace fpsparty::game
