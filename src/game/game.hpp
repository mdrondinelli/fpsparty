#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include "serial/serialize.hpp"
#include "serial/writer.hpp"
#include <bit>
#include <exception>

namespace fpsparty::game {
class Game;
class Player;
class Unique_game;
class Unique_player;

class Player {
  struct Impl;

public:
  struct Create_info;

  struct Input_state;

  constexpr Player() noexcept = default;

  constexpr explicit Player(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  std::uint32_t get_id() const noexcept;

  Input_state get_input_state() const noexcept;

  void set_input_state(const Input_state &input_state) const noexcept;

private:
  friend class Game;

  friend constexpr bool operator==(Player lhs, Player rhs) noexcept = default;

  friend struct std::hash<Player>;

  Impl *_impl{};
};

class Game {
  struct Impl;

public:
  struct Create_info;

  struct Simulate_player_input_info;

  struct Simulate_info;

  class Snapshotting_error;

  constexpr Game() noexcept = default;

  constexpr explicit Game(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  Player create_player(const Player::Create_info &info) const;

  void destroy_player(Player player) const noexcept;

  Unique_player create_player_unique(const Player::Create_info &info) const;

  std::size_t get_player_count() const noexcept;

  void simulate(const Simulate_info &info) const;

  void snapshot(serial::Writer &writer) const;

private:
  friend constexpr bool operator==(Game lhs, Game rhs) noexcept = default;

  friend Game create_game(const Create_info &info);

  friend void destroy_game(Game game) noexcept;

  friend struct std::hash<Game>;

  Impl *_impl{};
};

Game create_game(const Game::Create_info &info);

void destroy_game(Game game) noexcept;

struct Player::Create_info {};

struct Player::Input_state {
  bool move_left{};
  bool move_right{};
  bool move_forward{};
  bool move_backward{};
  float yaw{};
  float pitch{};
};

struct Game::Create_info {};

struct Game::Simulate_info {
  float duration;
};

class Game::Snapshotting_error : public std::exception {};

class Unique_player {
public:
  constexpr Unique_player() noexcept = default;

  constexpr explicit Unique_player(Player value, Game owner) noexcept
      : _value{value}, _owner{owner} {}

  constexpr Unique_player(Unique_player &&other) noexcept
      : _value{std::exchange(other._value, {})}, _owner{std::exchange(
                                                     other._owner, {})} {}

  Unique_player &operator=(Unique_player &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_player() {
    if (_owner && _value) {
      _owner.destroy_player(_value);
    }
  }

  const Player &operator*() const noexcept { return _value; }

  const Player *operator->() const noexcept { return &_value; }

private:
  constexpr void swap(Unique_player &other) noexcept {
    std::swap(_value, other._value);
  }

  Player _value{};
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

} // namespace fpsparty::game
namespace fpsparty::serial {
template <> struct Serializer<game::Player::Input_state> {
  void write(Writer &writer, const game::Player::Input_state &value) const {
    auto flags = std::uint8_t{};
    if (value.move_left) {
      flags |= 1 << 0;
    }
    if (value.move_right) {
      flags |= 1 << 1;
    }
    if (value.move_forward) {
      flags |= 1 << 2;
    }
    if (value.move_backward) {
      flags |= 1 << 3;
    }
    serialize<std::uint8_t>(writer, flags);
    serialize<float>(writer, value.yaw);
    serialize<float>(writer, value.pitch);
  }

  std::optional<game::Player::Input_state> read(Reader &reader) const {
    const auto flags = deserialize<std::uint8_t>(reader);
    if (!flags) {
      return std::nullopt;
    }
    const auto yaw = deserialize<float>(reader);
    if (!yaw) {
      return std::nullopt;
    }
    const auto pitch = deserialize<float>(reader);
    if (!pitch) {
      return std::nullopt;
    }
    return game::Player::Input_state{
        .move_left = (*flags & (1 << 0)) != 0,
        .move_right = (*flags & (1 << 1)) != 0,
        .move_forward = (*flags & (1 << 2)) != 0,
        .move_backward = (*flags & (1 << 3)) != 0,
        .yaw = *yaw,
        .pitch = *pitch,
    };
  }
};
} // namespace fpsparty::serial

namespace std {
template <> struct hash<fpsparty::game::Player> {
  constexpr std::size_t
  operator()(fpsparty::game::Player value) const noexcept {
    return static_cast<std::size_t>(std::bit_cast<std::uintptr_t>(value._impl));
  }
};

template <> struct hash<fpsparty::game::Game> {
  constexpr std::size_t operator()(fpsparty::game::Game value) const noexcept {
    return static_cast<std::size_t>(std::bit_cast<std::uintptr_t>(value._impl));
  }
};
} // namespace std

#endif
