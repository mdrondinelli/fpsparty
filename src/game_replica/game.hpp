#ifndef FPSPARTY_GAME_REPLICATED_GAME_HPP
#define FPSPARTY_GAME_REPLICATED_GAME_HPP

#include "game_core/humanoid_input_state.hpp"
#include "serial/reader.hpp"
#include <Eigen/Dense>
#include <memory_resource>
#include <utility>
#include <vector>

namespace fpsparty::game_replica {
class Humanoid {
public:
  constexpr Humanoid() noexcept = default;

  constexpr explicit Humanoid(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  std::uint32_t get_network_id() const noexcept;

  const game_core::Humanoid_input_state &get_input_state() const noexcept;

  std::optional<std::uint16_t> get_input_sequence_number() const noexcept;

  void set_input_state(
      const game_core::Humanoid_input_state &input_state) const noexcept;

  void set_input_state(const game_core::Humanoid_input_state &input_state,
                       std::uint16_t input_sequence_number) const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  friend constexpr bool operator==(Humanoid lhs,
                                   Humanoid rhs) noexcept = default;

private:
  friend class Game;

  struct Impl;

  Impl *_impl{};
};

class Projectile {
public:
  constexpr Projectile() noexcept = default;

  constexpr Projectile(void *impl) : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  std::uint32_t get_network_id() const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  friend constexpr bool operator==(Projectile lhs,
                                   Projectile rhs) noexcept = default;

private:
  friend class Game;

  struct Impl;

  Impl *_impl;
};

class Game {
public:
  struct Create_info;

  struct Simulate_info;

  class Snapshot_application_error;

  constexpr Game() noexcept = default;

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  void clear() const;

  void simulate(const Simulate_info &info) const;

  void apply_snapshot(serial::Reader &reader) const;

  std::pmr::vector<Humanoid>
  get_humanoids(std::pmr::memory_resource *memory_resource =
                    std::pmr::get_default_resource()) const;

  Humanoid get_humanoid_by_network_id(std::uint32_t network_id) const noexcept;

  std::pmr::vector<Projectile>
  get_projectiles(std::pmr::memory_resource *memory_resource =
                      std::pmr::get_default_resource()) const;

  Projectile
  get_projectile_by_network_id(std::uint32_t network_id) const noexcept;

private:
  struct Impl;

  constexpr explicit Game(Impl *impl) noexcept : _impl{impl} {}

  friend Game create_replicated_game(const Create_info &info);

  friend void destroy_replicated_game(Game replicated_game) noexcept;

  Impl *_impl{};
};

struct Game::Create_info {};

struct Game::Simulate_info {
  float duration;
};

class Game::Snapshot_application_error : std::exception {};

Game create_replicated_game(const Game::Create_info &info);

void destroy_replicated_game(Game replicated_game) noexcept;

class Unique_replicated_game {
public:
  constexpr Unique_replicated_game() noexcept = default;

  constexpr explicit Unique_replicated_game(Game value) noexcept
      : _value{value} {}

  constexpr Unique_replicated_game(Unique_replicated_game &&other) noexcept
      : _value{std::exchange(other._value, Game{})} {}

  Unique_replicated_game &operator=(Unique_replicated_game &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_replicated_game() {
    if (_value) {
      destroy_replicated_game(_value);
    }
  }

  const Game &operator*() const noexcept { return _value; }

  const Game *operator->() const noexcept { return &_value; }

private:
  constexpr void swap(Unique_replicated_game &other) noexcept {
    std::swap(_value, other._value);
  }

  Game _value{};
};

inline Unique_replicated_game
create_replicated_game_unique(const Game::Create_info &info) {
  return Unique_replicated_game{create_replicated_game(info)};
}
} // namespace fpsparty::game_replica

#endif
