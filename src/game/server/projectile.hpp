#ifndef FPSPARTY_GAME_PROJECTILE_HPP
#define FPSPARTY_GAME_PROJECTILE_HPP

#include "game/core/game_object.hpp"
#include "game/core/game_object_id.hpp"
#include "game/server/humanoid.hpp"
#include "rc.hpp"
#include <Eigen/Dense>

namespace fpsparty::game {
struct Projectile_create_info {
  rc::Weak<Humanoid> creator{};
  Eigen::Vector3f position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f velocity{Eigen::Vector3f::Zero()};
};

class Projectile : public Game_object, public rc::Object<Projectile> {
public:
  explicit Projectile(Game_object_id game_object_id,
                      const Projectile_create_info &info) noexcept;

  void on_remove() override;

  const rc::Weak<Humanoid> &get_creator() const noexcept;

  const Eigen::Vector3f &get_position() const noexcept;

  void set_position(const Eigen::Vector3f &position) noexcept;

  const Eigen::Vector3f &get_velocity() const noexcept;

  void set_velocity(const Eigen::Vector3f &velocity) noexcept;

private:
  struct Creator_remove_listener : Game_object_remove_listener {
    Projectile *projectile;

    explicit Creator_remove_listener(Projectile *projectile) noexcept;

    void on_remove_game_object() override;
  };

  rc::Weak<Humanoid> _creator{};
  Creator_remove_listener _creator_remove_listener;
  Eigen::Vector3f _position{Eigen::Vector3f::Zero()};
  Eigen::Vector3f _velocity{Eigen::Vector3f::Zero()};
};
} // namespace fpsparty::game

#endif
