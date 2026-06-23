#ifndef FPSPARTY_GAME_BLOCK_BOUNDS_HPP
#define FPSPARTY_GAME_BLOCK_BOUNDS_HPP

#include <cassert>
#include <vector>

#include <math/box.hpp>

#include "block.hpp"

namespace fpsparty::game {

class Block_bounds_registry {
public:
  Block_bounds_registry();

  math::box3 const &get(Block block) const noexcept {
    auto const idx = static_cast<int>(block);
    assert(idx < capacity);
    return _bounds[idx];
  }

private:
  void set(Block block, math::box3 bounds) noexcept {
    auto const idx = static_cast<int>(block);
    assert(idx < capacity);
    _bounds[idx] = bounds;
  }

  static auto constexpr capacity = 256;

  std::vector<math::box3> _bounds;
};

}

#endif // FPSPARTY_GAME_BLOCK_BOUNDS_HPP
