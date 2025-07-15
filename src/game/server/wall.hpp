#ifndef FPSPARTY_GAME_WALL_HPP
#define FPSPARTY_GAME_WALL_HPP

#include "game/core/entity.hpp"
#include <Eigen/Dense>
#include <span>
#include <vector>

namespace fpsparty::game {
struct Wall_create_info {
  float min_y{};
  float max_y{};
  std::span<const Eigen::Vector2f> corners{};
  bool loop{};
};

class Wall : public Entity, public rc::Object<Wall> {
public:
  explicit Wall(Entity_id entity_id, const Wall_create_info &info);

  void on_remove() override;

private:
  float _min_y{};
  float _max_y{};
  std::vector<Eigen::Vector2f> _corners{};
  bool _loop{};
};
} // namespace fpsparty::game

#endif
