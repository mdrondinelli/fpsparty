#ifndef FPSPARTY_VOXEL_GRID_GLSL
#define FPSPARTY_VOXEL_GRID_GLSL

#include "extensions.glsl"
#include "intersectors.glsl"
#include "srgb.glsl"

struct Rt_block_model {
  vec3 min;
  vec3 max;
};

Rt_block_model rt_block_models[] = {
  Rt_block_model(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0)),
  Rt_block_model(vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0)),
  Rt_block_model(vec3(0.0, 0.5, 0.0), vec3(1.0, 1.0, 1.0)),
};

vec3 rt_albedo_palette[] = {
  vec3(0.3), // generic
  color_code(0x5a4336), // dirt
};

struct Rt_cell {
  uint8_t model_index;
  uint8_t albedo_index;
};

struct Rt_chunk {
  Rt_cell cells[64];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
restrict readonly buffer Rt_grid {
  int min_chunk_x, min_chunk_y, min_chunk_z;
  int chunk_count_x, chunk_count_y, chunk_count_z;
  Rt_chunk chunks[];
};

bool rt_grid_get_cell(
    Rt_grid grid, ivec3 cell_coords, out Rt_cell out_cell) {
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

struct Rt_grid_hit {
  float t;
  vec3 normal;
  vec3 albedo;
};

bool rt_grid_trace(
    Rt_grid grid, vec3 origin, vec3 dir, out Rt_grid_hit hit) {
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
  // entry face of the current cell; origin starts inside the first cell, so
  // fall back to a normal opposing the ray until the first step
  float entry_t = 0.0;
  vec3 entry_normal = -dir;
  for (int i = 0; i < trace_max_steps; ++i) {
    Rt_cell rt_cell;
    if (!rt_grid_get_cell(grid, cell_coords, rt_cell)) {
      // oob -> terminate ray
      return false;
    }
    if (rt_cell.model_index == 1) {
      hit.t = entry_t;
      // ties between axes can leave a diagonal entry normal
      hit.normal = normalize(entry_normal);
      return true;
    } else if (rt_cell.model_index > 1) {
      const Rt_block_model model =
        rt_block_models[rt_cell.model_index];
      const vec3 model_space_center = mix(model.min, model.max, 0.5);
      const vec3 world_space_center = cell_coords + model_space_center;
      const vec3 half_extents = (model.max - model.min) * 0.5;
      vec3 hit_normal;
      const float t_hit =
        ray_aabb(origin, inv_dir, world_space_center, half_extents, hit_normal);
      if (!isinf(t_hit)) {
        hit.t = t_hit;
        hit.normal = hit_normal;
        hit.albedo = rt_albedo_palette[rt_cell.albedo_index];
        return true;
      }
    }
    const float next_t = min(t_max.x, min(t_max.y, t_max.z));
    if (next_t > trace_max_distance) {
      return false;
    }
    entry_t = next_t;
    entry_normal = vec3(0.0);
    if (t_max.x == next_t) {
      cell_coords.x += step_dir.x;
      t_max.x += t_delta.x;
      entry_normal.x = float(-step_dir.x);
    }
    if (t_max.y == next_t) {
      cell_coords.y += step_dir.y;
      t_max.y += t_delta.y;
      entry_normal.y = float(-step_dir.y);
    }
    if (t_max.z == next_t) {
      cell_coords.z += step_dir.z;
      t_max.z += t_delta.z;
      entry_normal.z = float(-step_dir.z);
    }
  }
  return false;
}

bool rt_grid_trace(Rt_grid grid, vec3 origin, vec3 dir) {
  Rt_grid_hit hit;
  return rt_grid_trace(grid, origin, dir, hit);
}

#endif
