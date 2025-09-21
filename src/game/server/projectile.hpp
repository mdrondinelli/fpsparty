#ifndef FPSPARTY_GAME_PROJECTILE_HPP
#define FPSPARTY_GAME_PROJECTILE_HPP

#include "game/core/entity.hpp"
#include "game/server/humanoid.hpp"
#include "net/core/entity_id.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
struct Projectile_create_info {
  Humanoid *creator{};
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
};

class Projectile : public Entity {
public:
  explicit Projectile(
    net::Entity_id entity_id, const Projectile_create_info &info) noexcept;

  void on_remove() override;

  Humanoid *get_creator() const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &position) noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  void set_velocity(const Eigen::Vector3f &velocity) noexcept;

private:
  struct Creator_remove_listener : Entity_remove_listener {
    Projectile *projectile;

    explicit Creator_remove_listener(Projectile *projectile) noexcept;

    void on_remove_entity() override;
  };

  Humanoid *_creator{};
  Creator_remove_listener _creator_remove_listener;
  Eigen::Vector3f _position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f _velocity{Eigen::Vector3f::Zero()};
};

class Projectile_dumper : public Entity_dumper {
public:
  Entity_type get_entity_type() const noexcept override;

  void dump_entity(serial::Writer &writer, const Entity &entity) const override;
};
} // namespace fpsparty::game

#endif
