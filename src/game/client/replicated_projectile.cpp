#include "replicated_projectile.hpp"
#include "net/core/entity_id.hpp"
#include "serial/serialize.hpp"

namespace fpsparty::game {
Replicated_projectile::Replicated_projectile(net::Entity_id entity_id) noexcept
    : Entity{Entity_type::projectile, entity_id} {}

void Replicated_projectile::on_remove() {}

Eigen::Vector3f const &Replicated_projectile::get_position() const noexcept {
  return _position;
}

void Replicated_projectile::set_position(
  Eigen::Vector3f const &value) noexcept {
  _position = value;
}

Eigen::Vector3f const &Replicated_projectile::get_velocity() const noexcept {
  return _velocity;
}

void Replicated_projectile::set_velocity(
  Eigen::Vector3f const &value) noexcept {
  _velocity = value;
}

Replicated_projectile_loader::Replicated_projectile_loader(
  std::pmr::memory_resource *memory_resource) noexcept
    : _factory{memory_resource} {}

Entity_owner<Entity>
Replicated_projectile_loader::create_entity(net::Entity_id entity_id) {
  return _factory.create(entity_id);
}

void Replicated_projectile_loader::load_entity(
  serial::Reader &reader, Entity &entity, Entity_world const &) const {
  using serial::deserialize;
  auto const projectile = dynamic_cast<Replicated_projectile *>(&entity);
  if (!projectile) {
    throw Replicated_projectile_load_error{};
  }
  auto const position = deserialize<Eigen::Vector3f>(reader);
  if (!position) {
    throw Replicated_projectile_load_error{};
  }
  auto const velocity = deserialize<Eigen::Vector3f>(reader);
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
