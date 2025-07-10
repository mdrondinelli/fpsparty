#ifndef FPSPARTY_GAME_REPLICA_PLAYER_HPP
#define FPSPARTY_GAME_REPLICA_PLAYER_HPP

#include "game_core/humanoid_input_state.hpp"
#include "game_replica/humanoid.hpp"
#include "rc.hpp"
#include <cstdint>

namespace fpsparty::game_replica {
class Player {
public:
  constexpr explicit Player(std::uint32_t network_id) noexcept
      : _network_id{network_id} {}

  std::uint32_t get_network_id() const noexcept;

  const rc::Weak<Humanoid> &get_humanoid() const noexcept;

  void set_humanoid(const rc::Weak<Humanoid> &value) noexcept;

  const game_core::Humanoid_input_state &get_input_state() const noexcept;

  void set_input_state(const game_core::Humanoid_input_state &value) noexcept;

  const std::optional<std::uint16_t> &
  get_input_sequence_number() const noexcept;

  void
  set_input_sequence_number(const std::optional<std::uint16_t> &value) noexcept;

private:
  std::uint32_t _network_id{};
  rc::Weak<Humanoid> _humanoid{};
  game_core::Humanoid_input_state _input_state{};
  std::optional<std::uint16_t> _input_sequence_number{};
};
} // namespace fpsparty::game_replica

#endif
