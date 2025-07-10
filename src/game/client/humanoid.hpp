#ifndef FPSPARTY_GAME_REPLICA_HUMANOID_HPP
#define FPSPARTY_GAME_REPLICA_HUMANOID_HPP

#include "game_core/humanoid_input_state.hpp"
#include <Eigen/Dense>
#include <cstdint>

namespace fpsparty::game_replica {
class Humanoid {
public:
  constexpr explicit Humanoid(std::uint32_t network_id) noexcept
      : _network_id{network_id} {}

  std::uint32_t get_network_id() const noexcept;

  const game_core::Humanoid_input_state &get_input_state() const noexcept;

  void set_input_state(const game_core::Humanoid_input_state &value) noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &value) noexcept;

private:
  std::uint32_t _network_id{};
  game_core::Humanoid_input_state _input_state{};
  Eigen::Vector3f _position{Eigen::Vector3f::Zero()};
};

} // namespace fpsparty::game_replica

#endif
