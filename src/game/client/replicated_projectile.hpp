#ifndef FPSPARTY_GAME_REPLICATED_PROJECTILE_HPP
#define FPSPARTY_GAME_REPLICATED_PROJECTILE_HPP

#include "game/core/entity.hpp"
#include "game/core/entity_id.hpp"
#include "game/core/entity_world.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
class Replicated_projectile : public Entity, rc::Object<Replicated_projectile> {
public:
  explicit Replicated_projectile(Entity_id entity_id) noexcept;

protected:
  void on_remove() override;

public:
  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &value) noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  void set_velocity(const Eigen::Vector3f &value) noexcept;

private:
  Eigen::Vector3f _position{};
  Eigen::Vector3f _velocity{};
};

class Replicated_projectile_load_error : public Entity_world_load_error {};

class Replicated_projectile_loader : public Entity_loader {
public:
  explicit Replicated_projectile_loader(
      std::pmr::memory_resource *memory_resource =
          std::pmr::get_default_resource()) noexcept;

  rc::Strong<Entity> create_entity(Entity_id entity_id) override;

  void load_entity(serial::Reader &reader, Entity &entity,
                   const Entity_world &world) const override;

  Entity_type get_entity_type() const noexcept override;

private:
  rc::Factory<Replicated_projectile> _factory{};
};
} // namespace fpsparty::game

#endif
