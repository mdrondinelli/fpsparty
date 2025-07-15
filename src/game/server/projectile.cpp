#include "projectile.hpp"
#include "game/core/entity_id.hpp"

namespace fpsparty::game {
Projectile::Creator_remove_listener::Creator_remove_listener(
    Projectile *projectile) noexcept
    : projectile{projectile} {}

void Projectile::Creator_remove_listener::on_remove_entity() {
  projectile->_creator = nullptr;
}

Projectile::Projectile(Entity_id entity_id,
                       const Projectile_create_info &info) noexcept
    : Entity{entity_id},
      _creator{info.creator},
      _creator_remove_listener{this},
      _position{info.position},
      _velocity{info.velocity} {
  const auto creator = _creator.lock();
  if (creator) {
    creator->add_remove_listener(&_creator_remove_listener);
  }
}

void Projectile::on_remove() {
  const auto creator = _creator.lock();
  _creator = nullptr;
  if (creator) {
    creator->remove_remove_listener(&_creator_remove_listener);
  }
}

const rc::Weak<Humanoid> &Projectile::get_creator() const noexcept {
  return _creator;
}

const Eigen::Vector3f &Projectile::get_position() const noexcept {
  return _position;
}

void Projectile::set_position(const Eigen::Vector3f &position) noexcept {
  _position = position;
}

const Eigen::Vector3f &Projectile::get_velocity() const noexcept {
  return _velocity;
}

void Projectile::set_velocity(const Eigen::Vector3f &velocity) noexcept {
  _velocity = velocity;
}
} // namespace fpsparty::game
