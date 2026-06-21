#ifndef FPSPARTY_CLIENT_BLOCK_MODEL_HPP
#define FPSPARTY_CLIENT_BLOCK_MODEL_HPP

#include <bitset>

#include <math/axis.hpp>

#include "block_mesh/block_mesh.hpp"

namespace fpsparty::client {

struct Block_model {
  bool occludes_neighbor(math::signed_axis3 normal) const noexcept {
    return neighbor_occlusion_flags[normal.index()];
  }

  Block_mesh mesh;
  std::bitset<6> neighbor_occlusion_flags;
};

} // namespace fpsparty::client

#endif
