#include "replicated_projectile.hpp"
#include "game/core/entity_id.hpp"
#include "serial/serialize.hpp"

namespace fpsparty::game {
Replicated_projectile::Replicated_projectile(Entity_id entity_id) noexcept
    : Entity{Entity_type::projectile, entity_id} {}

void Replicated_projectile::on_remove() {}

const Eigen::Vector3f &Replicated_projectile::get_position() const noexcept {
  return _position;
}

void Replicated_projectile::set_position(
    const Eigen::Vector3f &value) noexcept {
  _position = value;
}

const Eigen::Vector3f &Replicated_projectile::get_velocity() const noexcept {
  return _velocity;
}

void Replicated_projectile::set_velocity(
    const Eigen::Vector3f &value) noexcept {
  _velocity = value;
}

Replicated_projectile_loader::Replicated_projectile_loader(
    std::pmr::memory_resource *memory_resource) noexcept
    : _factory{memory_resource} {}

rc::Strong<Entity>
Replicated_projectile_loader::create_entity(Entity_id entity_id) {
  return _factory.create(entity_id);
}

void Replicated_projectile_loader::load_entity(serial::Reader &reader,
                                               Entity &entity,
                                               const Entity_world &) const {
  using serial::deserialize;
  const auto projectile = dynamic_cast<Replicated_projectile *>(&entity);
  if (!projectile) {
    throw Replicated_projectile_load_error{};
  }
  const auto position = deserialize<Eigen::Vector3f>(reader);
  if (!position) {
    throw Replicated_projectile_load_error{};
  }
  const auto velocity = deserialize<Eigen::Vector3f>(reader);
  if (!velocity) {
    throw Replicated_projectile_load_error{};
  }
  projectile->set_position(*position);
  projectile->set_velocity(*velocity);
}

Entity_type Replicated_projectile_loader::get_entity_type() const noexcept {
  return Entity_type::projectile;
}
} // namespace fpsparty::game
