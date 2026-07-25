#ifndef FPSPARTY_CLIENT_BLOCK_MODEL_HPP
#define FPSPARTY_CLIENT_BLOCK_MODEL_HPP

#include <bitset>
#include <cstdint>

#include <math/axis.hpp>

#include "block_mesh/block_mesh.hpp"

namespace fpsparty::client {

enum class Rt_block_shape : std::uint8_t {
  empty = 0,
  full = 1,
  bottom_slab = 2,
  top_slab = 3,
};

enum class Rt_color : std::uint8_t {
  generic = 0,
  red = 1,
  green = 2,
  blue = 3,
  white = 4,
  dirt = 5,
};

struct Block_model {
  bool occludes_neighbor(math::signed_axis3 normal) const noexcept {
    return neighbor_occlusion_flags[normal.index()];
  }

  Block_mesh mesh;
  std::bitset<6> neighbor_occlusion_flags;
  Rt_block_shape rt_shape{Rt_block_shape::full};
  Rt_color rt_color{Rt_color::generic};
  float emissivity_scale{};
};

} // namespace fpsparty::client

#endif
