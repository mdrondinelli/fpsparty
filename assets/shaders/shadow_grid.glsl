#ifndef FPSPARTY_VOXEL_GRID_GLSL
#define FPSPARTY_VOXEL_GRID_GLSL

#include "extensions.glsl"
#include "intersectors.glsl"

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


bool shadow_grid_get_cell(
    Shadow_grid grid, ivec3 cell_coords, out Shadow_cell out_cell) {
  const ivec3 chunk_coords = cell_coords >> 2;
  const ivec3 local_chunk =
    chunk_coords - ivec3(grid.min_chunk_x, grid.min_chunk_y, grid.min_chunk_z);
  const ivec3 chunk_counts =
    ivec3(grid.chunk_count_x, grid.chunk_count_y, grid.chunk_count_z);
  if (
      any(lessThan(local_chunk, ivec3(0))) ||
      any(greaterThanEqual(local_chunk, chunk_counts))) {
    return false;
  }
  const uint chunk_index = uint(
    local_chunk.x + local_chunk.y * chunk_counts.x +
    local_chunk.z * chunk_counts.x * chunk_counts.y);
  const ivec3 local_cell = cell_coords - (chunk_coords << 2);
  const uint cell_index =
    uint(local_cell.z * 16 + local_cell.y * 4 + local_cell.x);
  out_cell = grid.chunks[chunk_index].cells[cell_index];
  return true;
}

const int trace_max_steps = 192;
const float trace_max_distance = 64.0;

bool shadow_grid_trace(Shadow_grid grid, vec3 origin, vec3 dir) {
  ivec3 cell_coords = ivec3(floor(origin));
  const vec3 cell_offset = origin - vec3(cell_coords);
  const vec3 inv_dir = 1.0 / dir;
  ivec3 step_dir;
  vec3 t_max;
  vec3 t_delta;
  for (int axis = 0; axis < 3; ++axis) {
    if (dir[axis] > 0.0) {
      step_dir[axis] = 1;
      t_delta[axis] = inv_dir[axis];
      t_max[axis] = (1.0 - cell_offset[axis]) * t_delta[axis];
    } else if (dir[axis] < 0.0) {
      step_dir[axis] = -1;
      t_delta[axis] = -inv_dir[axis];
      t_max[axis] = cell_offset[axis] * t_delta[axis];
    } else {
      step_dir[axis] = 0;
      t_delta[axis] = 1.0 / 0.0;
      t_max[axis] = 1.0 / 0.0;
    }
  }
  for (int i = 0; i < trace_max_steps; ++i) {
    Shadow_cell shadow_cell;
    if (!shadow_grid_get_cell(grid, cell_coords, shadow_cell)) {
      // oob -> terminate ray
      return false;
    }
    if (shadow_cell.model_index == 1) {
      return true;
    } else if (shadow_cell.model_index > 1) {
      const Shadow_block_model model =
        shadow_block_models[shadow_cell.model_index];
      const vec3 model_space_center = mix(model.min, model.max, 0.5);
      const vec3 world_space_center = cell_coords + model_space_center;
      const vec3 half_extents = (model.max - model.min) * 0.5;
      const float t_hit =
        ray_aabb(origin, inv_dir, world_space_center, half_extents);
      if (!isinf(t_hit)) {
        return true;
      }
    }
    const float next_t = min(t_max.x, min(t_max.y, t_max.z));
    if (next_t > trace_max_distance) {
      return false;
    }
    if (t_max.x == next_t) {
      cell_coords.x += step_dir.x;
      t_max.x += t_delta.x;
    }
    if (t_max.y == next_t) {
      cell_coords.y += step_dir.y;
      t_max.y += t_delta.y;
    }
    if (t_max.z == next_t) {
      cell_coords.z += step_dir.z;
      t_max.z += t_delta.z;
    }
  }
  return false;
}

#endif
