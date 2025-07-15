#ifndef FPSPARTY_GAME_REPLICATED_PROJECTILE_HPP
#define FPSPARTY_GAME_REPLICATED_PROJECTILE_HPP

#include "game/core/entity.hpp"
#include "game/core/entity_id.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
class Replicated_projectile : public Entity {
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
} // namespace fpsparty::game

#endif
