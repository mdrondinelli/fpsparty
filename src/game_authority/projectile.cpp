#include "game.hpp"

namespace fpsparty::game {
struct Projectile::Impl {
  std::uint32_t network_id;
  Humanoid creator;
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
};

std::uint32_t Projectile::get_network_id() const noexcept {
  return _impl->network_id;
}

Humanoid Projectile::get_creator() const noexcept { return _impl->creator; }

const Eigen::Vector3f &Projectile::get_position() const noexcept {
  return _impl->position;
}

void Projectile::set_position(const Eigen::Vector3f &position) const noexcept {
  _impl->position = position;
}

const Eigen::Vector3f &Projectile::get_velocity() const noexcept {
  return _impl->velocity;
}

void Projectile::set_velocity(const Eigen::Vector3f &velocity) const noexcept {
  _impl->velocity = velocity;
}

Projectile::Impl *Projectile::new_impl(std::uint32_t network_id,
                                       const Create_info &info) {
  return new Impl{
      .network_id = network_id,
      .creator = info.creator,
      .position = info.position,
      .velocity = info.velocity,
  };
}

void Projectile::delete_impl(Impl *impl) { delete impl; }
} // namespace fpsparty::game
