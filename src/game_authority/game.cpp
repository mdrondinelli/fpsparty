#include "game.hpp"
#include "game_core/constants.hpp"
#include "game_core/humanoid_movement.hpp"
#include "game_core/projectile_movement.hpp"
#include "math/transformation_matrices.hpp"
#include "serial/serialize.hpp"
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <algorithm>

namespace fpsparty::game_authority {
struct Game::Impl {
  std::vector<Humanoid> humanoids{};
  std::vector<Projectile> projectiles{};
  std::uint32_t next_network_id{};
};

Game create_game(const Game::Create_info &) { return Game{new Game::Impl}; }

void destroy_game(Game game) noexcept {
  game.clear();
  delete game._impl;
}

void Game::clear() const noexcept {
  for (const auto &humanoid : _impl->humanoids) {
    Humanoid::delete_impl(humanoid._impl);
  }
  for (const auto &projectile : _impl->projectiles) {
    Projectile::delete_impl(projectile._impl);
  }
  _impl->humanoids.clear();
  _impl->projectiles.clear();
}

void Game::simulate(const Simulate_info &info) const {
  for (const auto &humanoid : _impl->humanoids) {
    humanoid.decrease_attack_cooldown(info.duration);
    if (humanoid.get_input_state().use_primary &&
        humanoid.get_attack_cooldown() == 0.0f) {
      const auto basis =
          (math::y_rotation_matrix(humanoid.get_input_state().yaw) *
           math::x_rotation_matrix(humanoid.get_input_state().pitch))
              .eval();
      const auto up = basis.col(1).head<3>().eval();
      const auto forward = basis.col(2).head<3>().eval();
      create_projectile({
          .creator = humanoid,
          .position = humanoid.get_position() + Eigen::Vector3f::UnitY() * 1.5f,
          .velocity = humanoid.get_velocity() +
                      game_core::constants::projectile_forward_speed * forward +
                      game_core::constants::projectile_up_speed * up,
      });
      humanoid.set_attack_cooldown(game_core::constants::attack_cooldown);
    }
    const auto movement_info = game_core::Humanoid_movement_simulation_info{
        .initial_position = humanoid.get_position(),
        .input_state = humanoid.get_input_state(),
        .duration = info.duration,
    };
    const auto movement_result =
        game_core::simulate_humanoid_movement(movement_info);
    humanoid.set_position(movement_result.final_position);
    humanoid.set_velocity(
        (movement_result.final_position - movement_info.initial_position) /
        info.duration);
  }
  for (const auto &projectile : _impl->projectiles) {
    const auto movement_result = game_core::simulate_projectile_movement({
        .initial_position = projectile.get_position(),
        .initial_velocity = projectile.get_velocity(),
        .duration = info.duration,
    });
    projectile.set_position(movement_result.final_position);
    projectile.set_velocity(movement_result.final_velocity);
  }
  for (const auto &projectile : get_projectiles()) {
    if (projectile.get_position().y() < 0.0f) {
      destroy_projectile(projectile);
    }
  }
  for (const auto &humanoid : get_humanoids()) {
    const auto humanoid_bounds = Eigen::AlignedBox3f{
        humanoid.get_position() -
            Eigen::Vector3f{
                game_core::constants::humanoid_half_width,
                0.0f,
                game_core::constants::humanoid_half_width,
            },
        humanoid.get_position() +
            Eigen::Vector3f{
                game_core::constants::humanoid_half_width,
                game_core::constants::humanoid_height,
                game_core::constants::humanoid_half_width,
            },
    };
    for (const auto &projectile : _impl->projectiles) {
      if (projectile.get_creator() == humanoid) {
        continue;
      }
      const auto projectile_half_extents = Eigen::Vector3f{
          game_core::constants::projectile_half_extent,
          game_core::constants::projectile_half_extent,
          game_core::constants::projectile_half_extent,
      };
      const auto projectile_bounds = Eigen::AlignedBox3f{
          projectile.get_position() - projectile_half_extents,
          projectile.get_position() + projectile_half_extents,
      };
      if (projectile_bounds.intersects(humanoid_bounds)) {
        destroy_humanoid(humanoid);
      }
    }
  }
}

void Game::snapshot(serial::Writer &writer) const {
  using serial::serialize;
  serialize<std::uint8_t>(writer, _impl->humanoids.size());
  for (const auto &humanoid : _impl->humanoids) {
    serialize<std::uint32_t>(writer, humanoid.get_network_id());
    serialize<float>(writer, humanoid.get_position().x());
    serialize<float>(writer, humanoid.get_position().y());
    serialize<float>(writer, humanoid.get_position().z());
    serialize<game_core::Humanoid_input_state>(writer,
                                               humanoid.get_input_state());
    serialize<std::optional<std::uint16_t>>(
        writer, humanoid.get_input_sequence_number());
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

Humanoid Game::create_humanoid(const Humanoid::Create_info &) const {
  const auto retval = Humanoid{Humanoid::new_impl(_impl->next_network_id++)};
  _impl->humanoids.emplace_back(retval);
  return retval;
}

void Game::destroy_humanoid(Humanoid humanoid) const noexcept {
  const auto it = std::ranges::find(_impl->humanoids, humanoid);
  if (it != _impl->humanoids.end()) {
    _impl->humanoids.erase(it);
    Humanoid::delete_impl(humanoid._impl);
  }
}

std::size_t Game::get_humanoid_count() const noexcept {
  return _impl->humanoids.size();
}

std::pmr::vector<Humanoid>
Game::get_humanoids(std::pmr::memory_resource *memory_resource) const {
  auto retval = std::pmr::vector<Humanoid>{memory_resource};
  retval.reserve(_impl->humanoids.size());
  retval.insert_range(retval.begin(), _impl->humanoids);
  return retval;
}

Projectile Game::create_projectile(const Projectile::Create_info &info) const {
  const auto retval =
      Projectile{Projectile::new_impl(_impl->next_network_id++, info)};
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
} // namespace fpsparty::game_authority
