#ifndef FPSPARTY_GAME_REPLICATED_PLAYER_HPP
#define FPSPARTY_GAME_REPLICATED_PLAYER_HPP

#include "game/client/replicated_humanoid.hpp"
#include "game/core/humanoid_input_state.hpp"
#include "rc.hpp"

namespace fpsparty::game {
class Replicated_player {
public:
  const rc::Weak<Replicated_humanoid> &get_humanoid() const noexcept;

  const Humanoid_input_state &get_input_state() const noexcept;

private:
  rc::Weak<Replicated_humanoid> _humanoid{};
  Humanoid_input_state _input_state{};
};
} // namespace fpsparty::game

#endif
