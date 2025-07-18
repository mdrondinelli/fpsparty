#include "replicated_humanoid.hpp"
#include "game/core/entity_id.hpp"

namespace fpsparty::game {
Replicated_humanoid::Replicated_humanoid(Entity_id entity_id) noexcept
    : Entity{Entity_type::humanoid, entity_id} {}

void Replicated_humanoid::on_remove() {}

const Humanoid_input_state &
Replicated_humanoid::get_input_state() const noexcept {
  return _input_state;
}

void Replicated_humanoid::set_input_state(
    const Humanoid_input_state &value) noexcept {
  _input_state = value;
}

const Eigen::Vector3f &Replicated_humanoid::get_position() const noexcept {
  return _position;
}

void Replicated_humanoid::set_position(const Eigen::Vector3f &value) noexcept {
  _position = value;
}

Replicated_humanoid_loader::Replicated_humanoid_loader(
    std::pmr::memory_resource *memory_resource) noexcept
    : _factory{memory_resource} {}

rc::Strong<Entity>
Replicated_humanoid_loader::create_entity(Entity_id entity_id) {
  return _factory.create(entity_id);
}

void Replicated_humanoid_loader::load_entity(serial::Reader &reader,
                                             Entity &entity,
                                             const Entity_world &) const {
  const auto humanoid = dynamic_cast<Replicated_humanoid *>(&entity);
  if (!humanoid) {
    throw Replicated_humanoid_load_error{};
  }
  const auto position = deserialize<Eigen::Vector3f>(reader);
  if (!position) {
    throw Replicated_humanoid_load_error{};
  }
  const auto input_state = deserialize<Humanoid_input_state>(reader);
  if (!input_state) {
    throw Replicated_humanoid_load_error{};
  }
  humanoid->set_position(*position);
  humanoid->set_input_state(*input_state);
}

Entity_type Replicated_humanoid_loader::get_entity_type() const noexcept {
  return Entity_type::humanoid;
}
} // namespace fpsparty::game
