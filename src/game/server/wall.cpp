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
} // namespace fpsparty::game
