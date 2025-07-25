#include "projectile.hpp"
#include "net/core/entity_id.hpp"

namespace fpsparty::game {
Projectile::Creator_remove_listener::Creator_remove_listener(
    Projectile *projectile) noexcept
    : projectile{projectile} {}

void Projectile::Creator_remove_listener::on_remove_entity() {
  projectile->_creator = nullptr;
}

Projectile::Projectile(net::Entity_id entity_id,
                       const Projectile_create_info &info) noexcept
    : Entity{Entity_type::projectile, entity_id},
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

Entity_type Projectile_dumper::get_entity_type() const noexcept {
  return Entity_type::projectile;
}

void Projectile_dumper::dump_entity(serial::Writer &writer,
                                    const Entity &entity) const {
  using serial::serialize;
  if (const auto projectile = dynamic_cast<const Projectile *>(&entity)) {
    serialize<Eigen::Vector3f>(writer, projectile->get_position());
    serialize<Eigen::Vector3f>(writer, projectile->get_velocity());
  }
}

} // namespace fpsparty::game
