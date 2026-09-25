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

// Held separately from shape indices for cache coherence (measured big win).
struct Rt_block_material {
  float16_t emissivity_scale;
  uint8_t color_index;
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

// One chunk's shape indices
struct Rt_block_shape_chunk {
  uint8_t shapes[64];
};

// Same chunking for block materials
struct Rt_block_material_chunk {
  Rt_block_material materials[64];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
restrict readonly buffer Rt_block_shape_grid {
  int min_chunk_x, min_chunk_y, min_chunk_z;
  int chunk_count_x, chunk_count_y, chunk_count_z;
  Rt_block_shape_chunk chunks[];
};

// Materials for the same chunks, in the same order. Lives in the same
// allocation as the shapes, at an offset the caller pushes.
layout(std430, buffer_reference, buffer_reference_align = 4)
restrict readonly buffer Rt_block_material_grid {
  Rt_block_material_chunk chunks[];
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
restrict buffer Rt_entity_head_grid {
  int heads[];
};

// One bit per cell: whether that cell's entity list holds anything. Eight
// bytes per chunk against the head grid's 256, so the answer the
// traversal almost always gets -- nothing here -- comes out of a sector
// that covers three neighbouring chunks too. The head grid is only
// touched once a bit says otherwise.
struct Rt_entity_mask_chunk {
  uint words[2];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
restrict buffer Rt_entity_mask_grid {
  Rt_entity_mask_chunk chunks[];
};

// Returns false when cell_coords falls outside the grid, which terminates
// a ray. The chunk and cell indices are returned separately: together they
// address the materials, and chunk * 64 + cell is the flat cell index
// entity_head_grid.heads uses.
bool rt_block_shape_grid_get(
    Rt_block_shape_grid block_shape_grid,
    ivec3 cell_coords,
    out uint out_shape_index,
    out uint out_chunk_index,
    out uint out_cell_index) {
  const ivec3 chunk_coords = cell_coords >> 2;
  const ivec3 local_chunk =
    chunk_coords - ivec3(
      block_shape_grid.min_chunk_x,
      block_shape_grid.min_chunk_y,
      block_shape_grid.min_chunk_z);
  const ivec3 chunk_counts =
    ivec3(
      block_shape_grid.chunk_count_x,
      block_shape_grid.chunk_count_y,
      block_shape_grid.chunk_count_z);
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
  out_chunk_index = chunk_index;
  out_cell_index = cell_index;
  out_shape_index = uint(block_shape_grid.chunks[chunk_index].shapes[cell_index]);
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

// Traversal that can be put down and picked up again. A warp runs until
// the last of its lanes terminates, and DDA trip counts vary by orders of
// magnitude, so a warp whose lanes have mostly finished is burning issue
// slots on masked-off threads. Carrying the state lets a caller stop and
// resume the survivors somewhere denser -- see direct_trace_brdf.comp.
//
// step_dir, t_delta and inv_dir are all derived from dir, so they are
// recomputed on resume rather than carried.
struct Rt_traversal {
  vec3 origin;
  vec3 dir;
  // All derived from dir, and all carried rather than recomputed per
  // call: with a small step budget rt_traverse is entered often enough
  // that its setup cost more than a step. Costs registers and no memory,
  // the state never being serialized.
  vec3 inv_dir;
  vec3 t_delta;
  ivec3 step_dir;
  // The face the ray entered the current cell through.
  vec3 entry_normal;
  vec3 t_max;
  ivec3 cell_coords;
  float entry_t;
  uint steps;
};

Rt_traversal rt_traversal_begin(vec3 origin, vec3 dir) {
  Rt_traversal traversal;
  traversal.origin = origin;
  traversal.dir = dir;
  traversal.inv_dir = 1.0 / dir;
  traversal.cell_coords = ivec3(floor(origin));
  const vec3 cell_offset = origin - vec3(traversal.cell_coords);
  const vec3 inv_dir = traversal.inv_dir;
  for (int axis = 0; axis < 3; ++axis) {
    if (dir[axis] > 0.0) {
      traversal.step_dir[axis] = 1;
      traversal.t_delta[axis] = inv_dir[axis];
      traversal.t_max[axis] = (1.0 - cell_offset[axis]) * inv_dir[axis];
    } else if (dir[axis] < 0.0) {
      traversal.step_dir[axis] = -1;
      traversal.t_delta[axis] = -inv_dir[axis];
      traversal.t_max[axis] = cell_offset[axis] * -inv_dir[axis];
    } else {
      traversal.step_dir[axis] = 0;
      traversal.t_delta[axis] = 1.0 / 0.0;
      traversal.t_max[axis] = 1.0 / 0.0;
    }
  }
  traversal.entry_t = 0.0;
  // origin starts inside the first cell, so fall back to a normal
  // opposing the ray until the first step
  traversal.entry_normal = -dir;
  traversal.steps = 0u;
  return traversal;
}

const uint rt_traverse_hit = 0u;
const uint rt_traverse_miss = 1u;
const uint rt_traverse_suspended = 2u;

uint rt_traverse(
    Rt_block_shape_grid block_shape_grid,
    Rt_block_material_grid block_material_grid,
    Rt_entities entities,
    Rt_entity_head_grid entity_head_grid,
    Rt_entity_mask_grid entity_mask_grid,
    Rt_entity_nodes entity_nodes,
    bool trace_entities,
    uint step_budget,
    inout Rt_traversal traversal,
    out Rt_hit hit) {
  const vec3 origin = traversal.origin;
  const vec3 dir = traversal.dir;
  const vec3 inv_dir = traversal.inv_dir;
  const ivec3 step_dir = traversal.step_dir;
  const vec3 t_delta = traversal.t_delta;
  for (uint budget = 0u; budget < step_budget; ++budget) {
    if (traversal.steps >= uint(trace_max_steps)) {
      return rt_traverse_miss;
    }
    uint shape_index;
    uint chunk_index;
    uint cell_index;
    if (!rt_block_shape_grid_get(
        block_shape_grid,
        traversal.cell_coords,
        shape_index,
        chunk_index,
        cell_index)) {
      // oob -> terminate ray
      return rt_traverse_miss;
    }
    const float next_t =
      min(traversal.t_max.x, min(traversal.t_max.y, traversal.t_max.z));
    if (shape_index == 1) {
      hit.t = traversal.entry_t;
      // ties between axes can leave a diagonal entry normal
      hit.normal = normalize(traversal.entry_normal);
      const Rt_block_material material =
        block_material_grid.chunks[chunk_index].materials[cell_index];
      const vec3 color = rt_color_palette[material.color_index];
      hit.albedo = color;
      hit.emissivity = color * float(material.emissivity_scale);
      return rt_traverse_hit;
    }
    float closest_t = 1.0 / 0.0;
    vec3 closest_normal;
    vec3 closest_albedo;
    vec3 closest_emissivity;
    if (shape_index > 1) {
      const Rt_block_shape shape = rt_block_shapes[shape_index];
      const vec3 shape_space_center = mix(shape.min, shape.max, 0.5);
      const vec3 world_space_center = traversal.cell_coords + shape_space_center;
      const vec3 half_extents = (shape.max - shape.min) * 0.5;
      vec3 hit_normal;
      const float t_hit =
        ray_aabb(origin, inv_dir, world_space_center, half_extents, hit_normal);
      if (!isinf(t_hit)) {
        closest_t = t_hit;
        closest_normal = hit_normal;
        const Rt_block_material material =
          block_material_grid.chunks[chunk_index].materials[cell_index];
        const vec3 color = rt_color_palette[material.color_index];
        closest_albedo = color;
        closest_emissivity = color * float(material.emissivity_scale);
      }
    }
    if (trace_entities &&
        (entity_mask_grid.chunks[chunk_index].words[cell_index >> 5u] &
         (1u << (cell_index & 31u))) != 0u) {
      int node_index = entity_head_grid.heads[chunk_index * 64u + cell_index];
      // Bounded by entities.count (not just `!= -1`): a valid, acyclic list
      // can't have more distinct nodes than there are entities, so this
      // guarantees termination even if a corrupted `next` pointer ever
      // formed a cycle.
      for (uint node_count = 0u;
          node_index != -1 && node_count < entities.count;
          ++node_count) {
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
      return rt_traverse_hit;
    }
    if (next_t > trace_max_distance) {
      return rt_traverse_miss;
    }
    traversal.entry_t = next_t;
    traversal.entry_normal = vec3(0.0);
    if (traversal.t_max.x == next_t) {
      traversal.cell_coords.x += step_dir.x;
      traversal.t_max.x += t_delta.x;
      traversal.entry_normal.x = float(-step_dir.x);
    }
    if (traversal.t_max.y == next_t) {
      traversal.cell_coords.y += step_dir.y;
      traversal.t_max.y += t_delta.y;
      traversal.entry_normal.y = float(-step_dir.y);
    }
    if (traversal.t_max.z == next_t) {
      traversal.cell_coords.z += step_dir.z;
      traversal.t_max.z += t_delta.z;
      traversal.entry_normal.z = float(-step_dir.z);
    }
    ++traversal.steps;
  }
  return rt_traverse_suspended;
}

bool trace_ray(
    Rt_block_shape_grid block_shape_grid,
    Rt_block_material_grid block_material_grid,
    Rt_entities entities,
    Rt_entity_head_grid entity_head_grid,
    Rt_entity_mask_grid entity_mask_grid,
    Rt_entity_nodes entity_nodes,
    vec3 origin,
    vec3 dir,
    bool trace_entities,
    out Rt_hit hit) {
  Rt_traversal traversal = rt_traversal_begin(origin, dir);
  return rt_traverse(
    block_shape_grid,
    block_material_grid,
    entities,
    entity_head_grid,
    entity_mask_grid,
    entity_nodes,
    trace_entities,
    uint(trace_max_steps),
    traversal,
    hit) == rt_traverse_hit;
}

bool trace_ray(
    Rt_block_shape_grid block_shape_grid,
    Rt_block_material_grid block_material_grid,
    Rt_entities entities,
    Rt_entity_head_grid entity_head_grid,
    Rt_entity_mask_grid entity_mask_grid,
    Rt_entity_nodes entity_nodes,
    vec3 origin,
    vec3 dir,
    bool trace_entities) {
  Rt_hit hit;
  return trace_ray(
    block_shape_grid,
    block_material_grid,
    entities,
    entity_head_grid,
    entity_mask_grid,
    entity_nodes,
    origin,
    dir,
    trace_entities,
    hit);
}

#endif
