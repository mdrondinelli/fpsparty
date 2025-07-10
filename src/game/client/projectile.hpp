#ifndef FPSPARTY_GAME_REPLICA_PROJECTILE_HPP
#define FPSPARTY_GAME_REPLICA_PROJECTILE_HPP

#include <Eigen/Dense>
#include <cstdint>

namespace fpsparty::game_replica {
class Projectile {
public:
  constexpr explicit Projectile(std::uint32_t network_id) noexcept
      : _network_id{network_id} {}

  std::uint32_t get_network_id() const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &value) noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  void set_velocity(const Eigen::Vector3f &value) noexcept;

private:
  std::uint32_t _network_id{};
  Eigen::Vector3f _position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f _velocity{Eigen::Vector3f::Zero()};
};

} // namespace fpsparty::game_replica

#endif
