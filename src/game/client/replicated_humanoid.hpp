#ifndef FPSPARTY_GAME_REPLICATED_HUMANOID_HPP
#define FPSPARTY_GAME_REPLICATED_HUMANOID_HPP

#include "game/core/entity.hpp"
#include "game/core/entity_world.hpp"
#include "net/core/input_state.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
class Replicated_humanoid : public Entity {
public:
  explicit Replicated_humanoid(net::Entity_id entity_id) noexcept;

protected:
  void on_remove() override;

public:
  const net::Input_state &get_input_state() const noexcept;

  void set_input_state(const net::Input_state &value) noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &value) noexcept;

private:
  net::Input_state _input_state{};
  Eigen::Vector3f _position{};
};

class Replicated_humanoid_load_error : public Entity_world_load_error {};

class Replicated_humanoid_loader : public Entity_loader {
public:
  explicit Replicated_humanoid_loader(
    std::pmr::memory_resource *memory_resource =
      std::pmr::get_default_resource()) noexcept;

  Entity_owner<Entity> create_entity(net::Entity_id entity_id) override;

  void load_entity(
    serial::Reader &reader,
    Entity &entity,
    const Entity_world &world) const override;

  Entity_type get_entity_type() const noexcept override;

private:
  Entity_factory<Replicated_humanoid> _factory{};
};
} // namespace fpsparty::game

#endif
