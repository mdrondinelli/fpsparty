#ifndef FPSPARTY_GAME_REPLICATED_GAME_HPP
#define FPSPARTY_GAME_REPLICATED_GAME_HPP

#include "game/client/replicated_world.hpp"
#include "serial/reader.hpp"
#include <exception>

namespace fpsparty::game {
struct Replicated_game_load_info {
  std::uint64_t tick_number{};
  serial::Reader *world_state_reader{};
  serial::Reader *player_states_reader{};
  std::uint8_t player_state_count{};
};

class Replicated_game_load_error : public std::exception {};

class Replicated_game {
public:
  void tick(float duration);

  void load(const Replicated_game_load_info &info);

  std::uint64_t get_tick_number() const noexcept;

  const Replicated_world &get_world() const noexcept;

  Replicated_world &get_world() noexcept;

private:
  std::uint64_t _tick_number{};
  Replicated_world _world{};
};
} // namespace fpsparty::game

#endif
