#ifndef FPSPARTY_RT_GLSL
#define FPSPARTY_RT_GLSL

#include "extensions.glsl"
#include "intersectors.glsl"
#include "srgb.glsl"

struct Rt_block_shape {
  vec3 min;
  vec3 max;
};

Rt_block_shape rt_block_shapes[] = {
  Rt_block_shape(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0)), // empty
  Rt_block_shape(vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0)), // full
  Rt_block_shape(vec3(0.0, 0.0, 0.0), vec3(1.0, 0.5, 1.0)), // bottom_slab
  Rt_block_shape(vec3(0.0, 0.5, 0.0), vec3(1.0, 1.0, 1.0)), // top_slab
};

vec3 rt_color_palette[] = {
  vec3(0.3), // generic
  vec3(1.0, 0.0, 0.0), // red
  vec3(0.0, 1.0, 0.0), // green
  vec3(0.0, 0.0, 1.0), // blue
  vec3(1.0, 1.0, 1.0), // white
  color_code(0x5a4336), // dirt
};

struct Rt_block_cell {
  uint8_t shape_index;
  uint8_t color_index;
  float emissivity_scale;
};

struct Rt_block_chunk {
  Rt_block_cell cells[64];
};

struct Rt_grid_entity {
  vec4 model[3];
  vec4 inverse_model[3];
  vec4 half_extents;
  vec4 albedo;
};

struct Rt_entity_node {
  uint entity_index;
  int next;
};

layout(std430, buffer_reference, buffer_reference_align = 4)
restrict readonly buffer Rt_block_grid {
  int min_chunk_x, min_chunk_y, min_chunk_z;
  int chunk_count_x, chunk_count_y, chunk_count_z;
  Rt_block_chunk chunks[];
};

layout(std430, buffer_reference, buffer_reference_align = 16)
restrict readonly buffer Rt_entities {
  uint count;
  Rt_grid_entity entities[];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
restrict buffer Rt_entity_nodes {
  uint allocation_count;
  Rt_entity_node nodes[];
};

layout(std430, buffer_reference, buffer_reference_align = 8)
restrict buffer Rt_entity_grid {
  int heads[];
};

bool rt_block_grid_get_cell(
    Rt_block_grid block_grid,
    ivec3 cell_coords,
    out Rt_block_cell out_cell,
    out uint out_cell_index) {
  const ivec3 chunk_coords = cell_coords >> 2;
  const ivec3 local_chunk =
    chunk_coords - ivec3(
      block_grid.min_chunk_x,
      block_grid.min_chunk_y,
      block_grid.min_chunk_z);
  const ivec3 chunk_counts =
    ivec3(
      block_grid.chunk_count_x,
      block_grid.chunk_count_y,
      block_grid.chunk_count_z);
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
  out_cell = block_grid.chunks[chunk_index].cells[cell_index];
  out_cell_index = chunk_index * 64 + cell_index;
  return true;
}

const int trace_max_steps = 192;
const float trace_max_distance = 64.0;

struct Rt_hit {
  float t;
  vec3 normal;
  vec3 albedo;
  vec3 emissivity;
};

bool trace_ray(
    Rt_block_grid block_grid,
    Rt_entities entities,
    Rt_entity_grid entity_grid,
    Rt_entity_nodes entity_nodes,
    vec3 origin,
    vec3 dir,
    bool trace_entities,
    out Rt_hit hit) {
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
    Rt_block_cell rt_block_cell;
    uint cell_index;
    if (!rt_block_grid_get_cell(
        block_grid, cell_coords, rt_block_cell, cell_index)) {
      // oob -> terminate ray
      return false;
    }
    const float next_t = min(t_max.x, min(t_max.y, t_max.z));
    if (rt_block_cell.shape_index == 1) {
      hit.t = entry_t;
      // ties between axes can leave a diagonal entry normal
      hit.normal = normalize(entry_normal);
      const vec3 color = rt_color_palette[rt_block_cell.color_index];
      hit.albedo = color;
      hit.emissivity = color * rt_block_cell.emissivity_scale;
      return true;
    }
    float closest_t = 1.0 / 0.0;
    vec3 closest_normal;
    vec3 closest_albedo;
    vec3 closest_emissivity;
    if (rt_block_cell.shape_index > 1) {
      const Rt_block_shape shape =
        rt_block_shapes[rt_block_cell.shape_index];
      const vec3 shape_space_center = mix(shape.min, shape.max, 0.5);
      const vec3 world_space_center = cell_coords + shape_space_center;
      const vec3 half_extents = (shape.max - shape.min) * 0.5;
      vec3 hit_normal;
      const float t_hit =
        ray_aabb(origin, inv_dir, world_space_center, half_extents, hit_normal);
      if (!isinf(t_hit)) {
        closest_t = t_hit;
        closest_normal = hit_normal;
        const vec3 color = rt_color_palette[rt_block_cell.color_index];
        closest_albedo = color;
        closest_emissivity = color * rt_block_cell.emissivity_scale;
      }
    }
    if (trace_entities) {
      int node_index = entity_grid.heads[cell_index];
      while (node_index != -1) {
        const Rt_entity_node node = entity_nodes.nodes[node_index];
        const Rt_grid_entity entity = entities.entities[node.entity_index];
        const vec3 model_origin = vec3(
          dot(entity.inverse_model[0].xyz, origin) + entity.inverse_model[0].w,
          dot(entity.inverse_model[1].xyz, origin) + entity.inverse_model[1].w,
          dot(entity.inverse_model[2].xyz, origin) + entity.inverse_model[2].w);
        const vec3 model_dir = vec3(
          dot(entity.inverse_model[0].xyz, dir),
          dot(entity.inverse_model[1].xyz, dir),
          dot(entity.inverse_model[2].xyz, dir));
        vec3 model_normal;
        const float entity_t = ray_aabb(
          model_origin,
          1.0 / model_dir,
          vec3(0.0),
          entity.half_extents.xyz,
          model_normal);
        if (entity_t <= next_t && entity_t < closest_t) {
          closest_t = entity_t;
          closest_normal = normalize(vec3(
            dot(entity.model[0].xyz, model_normal),
            dot(entity.model[1].xyz, model_normal),
            dot(entity.model[2].xyz, model_normal)));
          closest_albedo = entity.albedo.xyz;
          closest_emissivity = vec3(0.0);
        }
        node_index = node.next;
      }
    }
    if (!isinf(closest_t)) {
      hit.t = closest_t;
      hit.normal = closest_normal;
      hit.albedo = closest_albedo;
      hit.emissivity = closest_emissivity;
      return true;
    }
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

bool trace_ray(
    Rt_block_grid block_grid,
    Rt_entities entities,
    Rt_entity_grid entity_grid,
    Rt_entity_nodes entity_nodes,
    vec3 origin,
    vec3 dir,
    bool trace_entities) {
  Rt_hit hit;
  return trace_ray(
    block_grid,
    entities,
    entity_grid,
    entity_nodes,
    origin,
    dir,
    trace_entities,
    hit);
}

#endif
