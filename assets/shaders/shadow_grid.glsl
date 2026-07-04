#ifndef FPSPARTY_VOXEL_GRID_GLSL
#define FPSPARTY_VOXEL_GRID_GLSL

#include "extensions.glsl"

struct Shadow_block_model {
  vec3 min;
  vec3 max;
};

Shadow_block_model shadow_block_models[] = {
  Shadow_block_model(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0)),
  Shadow_block_model(vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0)),
  Shadow_block_model(vec3(0.0, 0.5, 0.0), vec3(1.0, 1.0, 1.0)),
};

struct Shadow_cell {
  int8_t model_index; // 0 = empty, 1 = full, 2+ = index
};

struct Shadow_chunk {
  Shadow_cell cells[64];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
restrict readonly buffer Shadow_grid {
  int min_chunk_x, min_chunk_y, min_chunk_z;
  int chunk_count_x, chunk_count_y, chunk_count_z;
  Shadow_chunk chunks[];
};

uint get_shadow_block_model_index(Shadow_grid grid, ivec3 cell) {
  const ivec3 chunk_coords = cell >> 2;
  const ivec3 local_chunk =
    chunk_coords - ivec3(grid.min_chunk_x, grid.min_chunk_y, grid.min_chunk_z);
  const ivec3 chunk_counts =
    ivec3(grid.chunk_count_x, grid.chunk_count_y, grid.chunk_count_z);
  if (
      any(lessThan(local_chunk, ivec3(0))) ||
      any(greaterThanEqual(local_chunk, chunk_counts))) {
    return 0;
  }
  const uint chunk_index = uint(
    local_chunk.x + local_chunk.y * chunk_counts.x +
    local_chunk.z * chunk_counts.x * chunk_counts.y);
  const ivec3 local_cell = cell - (chunk_coords << 2);
  const uint cell_index =
    uint(local_cell.z * 16 + local_cell.y * 4 + local_cell.x);
  return grid.chunks[chunk_index].cells[cell_index].model_index;
}

#endif
