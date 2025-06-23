#ifndef FPSPARTY_GAME_REPLICATED_GAME_HPP
#define FPSPARTY_GAME_REPLICATED_GAME_HPP

#include "game/game.hpp"
#include "serial/reader.hpp"
#include <Eigen/Dense>
#include <memory_resource>
#include <utility>
#include <vector>

namespace fpsparty::game {
class Replicated_player {
public:
  constexpr Replicated_player() noexcept = default;

  constexpr explicit Replicated_player(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  std::uint32_t get_network_id() const noexcept;

  const Player::Input_state &get_input_state() const noexcept;

  std::optional<std::uint16_t> get_input_sequence_number() const noexcept;

  void set_input_state(const Player::Input_state &input_state) const noexcept;

  void set_input_state(const Player::Input_state &input_state,
                       std::uint16_t input_sequence_number) const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  friend constexpr bool operator==(Replicated_player lhs,
                                   Replicated_player rhs) noexcept = default;

private:
  friend class Replicated_game;

  struct Impl;

  Impl *_impl{};
};

class Replicated_projectile {
public:
  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  std::uint32_t get_network_id() const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

private:
  struct Impl;

  Impl *_impl;
};

class Replicated_game {
public:
  struct Create_info;

  struct Simulate_info;

  class Snapshot_application_error;

  constexpr Replicated_game() noexcept = default;

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  void clear() const;

  void simulate(const Simulate_info &info) const;

  void apply_snapshot(serial::Reader &reader) const;

  std::pmr::vector<Replicated_player>
  get_players(std::pmr::memory_resource *memory_resource =
                  std::pmr::get_default_resource()) const;

  Replicated_player
  get_player_by_network_id(std::uint32_t network_id) const noexcept;

  std::pmr::vector<Replicated_projectile>
  get_projectiles(std::pmr::memory_resource *memory_resource =
                      std::pmr::get_default_resource()) const;

private:
  struct Impl;

  constexpr explicit Replicated_game(Impl *impl) noexcept : _impl{impl} {}

  friend Replicated_game create_replicated_game(const Create_info &info);

  friend void destroy_replicated_game(Replicated_game replicated_game) noexcept;

  Impl *_impl{};
};

struct Replicated_game::Create_info {};

struct Replicated_game::Simulate_info {
  float duration;
};

class Replicated_game::Snapshot_application_error : std::exception {};

Replicated_game
create_replicated_game(const Replicated_game::Create_info &info);

void destroy_replicated_game(Replicated_game replicated_game) noexcept;

class Unique_replicated_game {
public:
  constexpr Unique_replicated_game() noexcept = default;

  constexpr explicit Unique_replicated_game(Replicated_game value) noexcept
      : _value{value} {}

  constexpr Unique_replicated_game(Unique_replicated_game &&other) noexcept
      : _value{std::exchange(other._value, Replicated_game{})} {}

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

  const Replicated_game &operator*() const noexcept { return _value; }

  const Replicated_game *operator->() const noexcept { return &_value; }

private:
  constexpr void swap(Unique_replicated_game &other) noexcept {
    std::swap(_value, other._value);
  }

  Replicated_game _value{};
};

inline Unique_replicated_game
create_replicated_game_unique(const Replicated_game::Create_info &info) {
  return Unique_replicated_game{create_replicated_game(info)};
}
} // namespace fpsparty::game

#endif
