#ifndef FPSPARTY_GAME_REPLICATED_GAME_HPP
#define FPSPARTY_GAME_REPLICATED_GAME_HPP

#include "game/client/replicated_world.hpp"
#include "game/core/sequence_number.hpp"
#include "serial/reader.hpp"
#include <exception>

namespace fpsparty::game {
struct Replicated_game_load_info {
  Sequence_number tick_number{};
  serial::Reader *public_state_reader{};
  serial::Reader *player_state_reader{};
  std::uint8_t player_state_count{};
};

class Replicated_game_load_error : public std::exception {};

class Replicated_game {
public:
  void tick(float duration);

  void load(const Replicated_game_load_info &info);

  const Replicated_world &get_world() const noexcept;

  Replicated_world &get_world() noexcept;

  Sequence_number get_tick_number() const noexcept;

private:
  Replicated_world _world{};
  Sequence_number _tick_number{};
};
} // namespace fpsparty::game

#endif
