#ifndef FPSPARTY_GAME_REPLICATED_HUMANOID_HPP
#define FPSPARTY_GAME_REPLICATED_HUMANOID_HPP

#include "game/core/entity.hpp"
#include "game/core/entity_world.hpp"
#include "game/core/humanoid_input_state.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
class Replicated_humanoid : public Entity,
                            public rc::Object<Replicated_humanoid> {
public:
  explicit Replicated_humanoid(Entity_id entity_id) noexcept;

protected:
  void on_remove() override;

public:
  const Humanoid_input_state &get_input_state() const noexcept;

  void set_input_state(const Humanoid_input_state &value) noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &value) noexcept;

private:
  Humanoid_input_state _input_state{};
  Eigen::Vector3f _position{};
};

class Replicated_humanoid_load_error : public Entity_world_load_error {};

class Replicated_humanoid_loader : public Entity_loader {
public:
  explicit Replicated_humanoid_loader(
      std::pmr::memory_resource *memory_resource =
          std::pmr::get_default_resource()) noexcept;

  rc::Strong<Entity> create_entity(Entity_id entity_id) override;

  void load_entity(serial::Reader &reader, Entity &entity,
                   const Entity_world &world) const override;

  Entity_type get_entity_type() const noexcept override;

private:
  rc::Factory<Replicated_humanoid> _factory{};
};
} // namespace fpsparty::game

#endif
