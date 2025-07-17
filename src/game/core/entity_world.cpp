#include "entity_world.hpp"
#include "algorithms/unordered_erase.hpp"
#include "game/core/entity.hpp"
#include "game/server/humanoid.hpp"
#include "game/server/projectile.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
void Entity_world::dump(serial::Writer &writer) const {
  using serial::serialize;
  const auto humanoids = get_entities_of_type<Humanoid>();
  serialize<std::uint8_t>(writer, humanoids.size());
  for (const auto &humanoid : humanoids) {
    serialize<Entity_id>(writer, humanoid->get_entity_id());
    serialize<Eigen::Vector3f>(writer, humanoid->get_position());
    serialize<Humanoid_input_state>(writer, humanoid->get_input_state());
  }
  const auto projectiles = get_entities_of_type<Projectile>();
  serialize<std::uint16_t>(writer, projectiles.size());
  for (const auto &projectile : projectiles) {
    serialize<Entity_id>(writer, projectile->get_entity_id());
    serialize<Eigen::Vector3f>(writer, projectile->get_position());
    serialize<Eigen::Vector3f>(writer, projectile->get_velocity());
  }
}

bool Entity_world::add(const rc::Strong<Entity> &entity) {
  const auto it = std::ranges::find(_entities, entity);
  if (it == _entities.end()) {
    _entities.emplace_back(entity);
    return true;
  } else {
    return false;
  }
}

bool Entity_world::remove(const rc::Strong<Entity> &entity) noexcept {
  const auto it = std::ranges::find(_entities, entity);
  if (it != _entities.end()) {
    detail::on_remove_entity(**it);
    algorithms::unordered_erase_at(_entities, it);
    return true;
  } else {
    return false;
  }
}
} // namespace fpsparty::game
