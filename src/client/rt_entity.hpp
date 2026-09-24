#ifndef FPSPARTY_CLIENT_RT_ENTITY_HPP
#define FPSPARTY_CLIENT_RT_ENTITY_HPP

#include "math/vec.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace fpsparty::client {

auto constexpr rt_entity_node_size = 2 * sizeof(std::uint32_t);
auto constexpr rt_entity_nodes_per_chunk = std::size_t{1000};
auto constexpr rt_cells_per_chunk = std::size_t{64};

// Must match Rt_entity in rt.glsl exactly.
struct alignas(16) Rt_entity {
  std::array<float, 12> model;
  std::array<float, 12> inverse_model;
  math::vec4 half_extents;
  math::vec4 albedo;
};

static_assert(sizeof(Rt_entity) == 128);
static_assert(offsetof(Rt_entity, inverse_model) == 48);
static_assert(offsetof(Rt_entity, half_extents) == 96);
static_assert(offsetof(Rt_entity, albedo) == 112);

// Size of one chunk's entity occupancy mask: one bit per cell, as the two
// uint words rt.glsl's Rt_entity_mask_chunk declares.
auto constexpr rt_entity_mask_chunk_size = 2 * sizeof(std::uint32_t);

static_assert(rt_cells_per_chunk == rt_entity_mask_chunk_size * 8);

struct Rt_entity_binning_buffer_layout {
  std::size_t mask_offset;
  std::size_t grid_offset;
  std::size_t nodes_offset;
  std::size_t size;
};

// One allocation, three regions. The masks come first: they are the
// smallest and the ones traversal reads every step.
inline Rt_entity_binning_buffer_layout
make_rt_entity_binning_buffer_layout(std::size_t chunk_count) {
  auto const mask_offset = std::size_t{};
  auto const mask_size = chunk_count * rt_entity_mask_chunk_size;
  auto const grid_offset = mask_offset + mask_size;
  auto const grid_size =
    chunk_count * rt_cells_per_chunk * sizeof(std::int32_t);
  auto const nodes_offset = grid_offset + grid_size;
  assert(grid_offset % 8 == 0);
  assert(nodes_offset % 8 == 0);
  auto const nodes_size = sizeof(std::uint32_t) +
                           chunk_count * rt_entity_nodes_per_chunk *
                             rt_entity_node_size;
  return {
    .mask_offset = mask_offset,
    .grid_offset = grid_offset,
    .nodes_offset = nodes_offset,
    .size = nodes_offset + nodes_size,
  };
}

} // namespace fpsparty::client

#endif
