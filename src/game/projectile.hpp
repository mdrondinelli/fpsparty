#ifndef FPSPARTY_GAME_PROJECTILE_HPP
#define FPSPARTY_GAME_PROJECTILE_HPP

#include "game/entity.hpp"
#include "game/humanoid.hpp"
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
    net::Entity_id entity_id, Projectile_create_info const &info) noexcept;

  void on_remove() override;

  Humanoid *get_creator() const noexcept;

  Eigen::Vector3f const &get_position() const noexcept;

  void set_position(Eigen::Vector3f const &position) noexcept;

  Eigen::Vector3f const &get_velocity() const noexcept;

  void set_velocity(Eigen::Vector3f const &velocity) noexcept;

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

  void dump_entity(serial::Writer &writer, Entity const &entity) const override;
};
} // namespace fpsparty::game

#endif
