#include "game.hpp"

namespace fpsparty::game {
struct Projectile::Impl {
  std::uint32_t network_id;
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
};

std::uint32_t Projectile::get_network_id() const noexcept {
  return _impl->network_id;
}

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

Projectile::Impl *Projectile::new_impl(std::uint32_t network_id) {
  return new Impl{.network_id = network_id};
}

void Projectile::delete_impl(Impl *impl) { delete impl; }
} // namespace fpsparty::game
