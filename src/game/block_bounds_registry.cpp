#include "block_bounds.hpp"

#include <math/vec.hpp>

#include "block.hpp"

namespace fpsparty::game {

Block_bounds_registry::Block_bounds_registry() {
  _bounds.resize(capacity);
  for (auto &bounds : _bounds) {
    bounds = math::box3{math::vec3::Zero(), math::vec3::Ones()};
  }
  set(
    Block::conveyor,
    math::box3{
      math::vec3{0.0f, 0.5f, 0.0f},
      math::vec3{1.0f, 1.0f, 1.0f},
    });
}

} // namespace fpsparty::game
