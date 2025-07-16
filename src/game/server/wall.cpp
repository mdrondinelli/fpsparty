#include "wall.hpp"

namespace fpsparty::game {
Wall::Wall(Entity_id entity_id, const Wall_create_info &info)
    : Entity{entity_id},
      _min_y{info.min_y},
      _max_y{info.max_y},
      _loop{info.loop} {
  _corners.assign_range(info.corners);
}

void Wall::on_remove() {}

float Wall::get_min_y() const noexcept { return _min_y; }

float Wall::get_max_y() const noexcept { return _max_y; }

std::span<const Eigen::Vector2f> Wall::get_corners() const noexcept {
  return _corners;
}

bool Wall::is_loop() const noexcept { return _loop; }
} // namespace fpsparty::game
