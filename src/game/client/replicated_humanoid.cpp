#include "replicated_humanoid.hpp"

namespace fpsparty::game {
void Replicated_humanoid::set_input_state(
    const Humanoid_input_state &value) noexcept {
  _input_state = value;
}
} // namespace fpsparty::game
