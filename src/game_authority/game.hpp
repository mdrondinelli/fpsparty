#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include "game_core/humanoid_input_state.hpp"
#include "serial/writer.hpp"
#include <Eigen/Dense>
#include <bit>
#include <exception>
#include <memory_resource>

namespace fpsparty::game_authority {
class Game;
class Humanoid;
class Projectile;
class Unique_game;
class Unique_humanoid;

class Humanoid {
public:
  struct Create_info;

  constexpr Humanoid() noexcept = default;

  constexpr explicit Humanoid(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  std::uint32_t get_network_id() const noexcept;

  game_core::Humanoid_input_state get_input_state() const noexcept;

  void set_input_state(const game_core::Humanoid_input_state &input_state,
                       std::uint16_t input_sequence_number,
                       bool input_fresh) const noexcept;

  std::optional<std::uint16_t> get_input_sequence_number() const noexcept;

  void increment_input_sequence_number() const noexcept;

  bool is_input_stale() const noexcept;

  void mark_input_stale() const noexcept;

  float get_attack_cooldown() const noexcept;

  void set_attack_cooldown(float value) const noexcept;

  void decrease_attack_cooldown(float amount) const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &position) const noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  void set_velocity(const Eigen::Vector3f &velocity) const noexcept;

private:
  friend class Game;

  friend constexpr bool operator==(Humanoid lhs,
                                   Humanoid rhs) noexcept = default;

  friend struct std::hash<Humanoid>;

  struct Impl;

  static Impl *new_impl(std::uint32_t network_id);

  static void delete_impl(Impl *impl);

  Impl *_impl{};
};

class Projectile {
public:
  struct Create_info;

  constexpr Projectile() noexcept = default;

  constexpr explicit Projectile(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  std::uint32_t get_network_id() const noexcept;

  Humanoid get_creator() const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &position) const noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  void set_velocity(const Eigen::Vector3f &velocity) const noexcept;

private:
  friend class Game;

  friend constexpr bool operator==(Projectile lhs,
                                   Projectile rhs) noexcept = default;

  friend struct std::hash<Projectile>;

  struct Impl;

  static Impl *new_impl(std::uint32_t network_id, const Create_info &info);

  static void delete_impl(Impl *impl);

  Impl *_impl{};
};

class Game {
  struct Impl;

public:
  struct Create_info;

  struct Simulate_info;

  class Snapshotting_error;

  constexpr Game() noexcept = default;

  constexpr explicit Game(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  void clear() const noexcept;

  void simulate(const Simulate_info &info) const;

  void snapshot(serial::Writer &writer) const;

  Humanoid create_humanoid(const Humanoid::Create_info &info) const;

  void destroy_humanoid(Humanoid humanoid) const noexcept;

  std::size_t get_humanoid_count() const noexcept;

  std::pmr::vector<Humanoid>
  get_humanoids(std::pmr::memory_resource *memory_resource =
                    std::pmr::get_default_resource()) const;

  Projectile create_projectile(const Projectile::Create_info &info) const;

  void destroy_projectile(Projectile projectile) const noexcept;

  std::pmr::vector<Projectile>
  get_projectiles(std::pmr::memory_resource *memory_resource =
                      std::pmr::get_default_resource()) const;

private:
  friend constexpr bool operator==(Game lhs, Game rhs) noexcept = default;

  friend Game create_game(const Create_info &info);

  friend void destroy_game(Game game) noexcept;

  friend struct std::hash<Game>;

  Impl *_impl{};
};

Game create_game(const Game::Create_info &info);

void destroy_game(Game game) noexcept;

struct Humanoid::Create_info {};

struct Projectile::Create_info {
  Humanoid creator;
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
};

struct Game::Create_info {};

struct Game::Simulate_info {
  float duration;
};

class Game::Snapshotting_error : public std::exception {};

class Unique_humanoid {
public:
  constexpr Unique_humanoid() noexcept = default;

  constexpr explicit Unique_humanoid(Humanoid value, Game owner) noexcept
      : _value{value}, _owner{owner} {}

  constexpr Unique_humanoid(Unique_humanoid &&other) noexcept
      : _value{std::exchange(other._value, {})},
        _owner{std::exchange(other._owner, {})} {}

  Unique_humanoid &operator=(Unique_humanoid &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_humanoid() {
    if (_owner && _value) {
      _owner.destroy_humanoid(_value);
    }
  }

  const Humanoid &operator*() const noexcept { return _value; }

  const Humanoid *operator->() const noexcept { return &_value; }

private:
  constexpr void swap(Unique_humanoid &other) noexcept {
    std::swap(_value, other._value);
  }

  Humanoid _value{};
  Game _owner{};
};

class Unique_game {
public:
  constexpr Unique_game() noexcept = default;

  constexpr explicit Unique_game(Game value) noexcept : _value{value} {}

  constexpr Unique_game(Unique_game &&other) noexcept
      : _value{std::exchange(other._value, Game{})} {}

  Unique_game &operator=(Unique_game &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_game() {
    if (_value) {
      destroy_game(_value);
    }
  }

  const Game &operator*() const noexcept { return _value; }

  const Game *operator->() const noexcept { return &_value; }

private:
  constexpr void swap(Unique_game &other) noexcept {
    std::swap(_value, other._value);
  }

  Game _value{};
};

inline Unique_game create_game_unique(const Game::Create_info &info) {
  return Unique_game{create_game(info)};
}

} // namespace fpsparty::game_authority

namespace std {
template <> struct hash<fpsparty::game_authority::Humanoid> {
  constexpr std::size_t
  operator()(fpsparty::game_authority::Humanoid value) const noexcept {
    return static_cast<std::size_t>(std::bit_cast<std::uintptr_t>(value._impl));
  }
};

template <> struct hash<fpsparty::game_authority::Projectile> {
  constexpr std::size_t
  operator()(fpsparty::game_authority::Projectile value) const noexcept {
    return static_cast<std::size_t>(std::bit_cast<std::uintptr_t>(value._impl));
  }
};

template <> struct hash<fpsparty::game_authority::Game> {
  constexpr std::size_t
  operator()(fpsparty::game_authority::Game value) const noexcept {
    return static_cast<std::size_t>(std::bit_cast<std::uintptr_t>(value._impl));
  }
};
} // namespace std

#endif
