#ifndef FPSPARTY_GAME_GAME_HPP
#define FPSPARTY_GAME_GAME_HPP

#include <span>

namespace fpsparty::game {
class Game;
class Player;
class Unique_game;
class Unique_player;

class Player {
  struct Impl;

public:
  struct Create_info;

  constexpr Player() noexcept = default;

  constexpr explicit Player(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

private:
  friend class Game;

  Impl *_impl{};
};

class Game {
  struct Impl;

public:
  struct Create_info;

  struct Simulate_player_input_info;

  struct Simulate_info;

  constexpr Game() noexcept = default;

  constexpr explicit Game(void *impl) noexcept
      : _impl{static_cast<Impl *>(impl)} {}

  constexpr operator bool() const noexcept { return _impl != nullptr; }

  constexpr explicit operator void *() const noexcept { return _impl; }

  Player create_player(const Player::Create_info &info) const;

  void destroy_player(Player player) const noexcept;

  Unique_player create_player_unique(const Player::Create_info &info) const;

  void simulate(const Simulate_info &info) const;

private:
  friend Game create_game(const Create_info &info);

  friend void destroy_game(Game game) noexcept;

  Impl *_impl{};
};

Game create_game(const Game::Create_info &info);

void destroy_game(Game game) noexcept;

struct Player::Create_info {};

struct Game::Create_info {};

struct Game::Simulate_player_input_info {
  Player player;
  bool move_left;
  bool move_right;
  bool move_forward;
  bool move_backward;
};

struct Game::Simulate_info {
  std::span<const Simulate_player_input_info> player_inputs;
  float duration;
};

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

#endif
