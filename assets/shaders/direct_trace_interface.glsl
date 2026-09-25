#ifndef FPSPARTY_DIRECT_TRACE_INTERFACE_GLSL
#define FPSPARTY_DIRECT_TRACE_INTERFACE_GLSL

#include "direct_common.glsl"
#include "rt.glsl"

// Shared by the queue-driven environment traces; offsets match
// Direct_trace_pass::execute. The BRDF trace has no queue and declares its
// own block.
layout(push_constant) uniform Push_constants {
  layout(offset = 0) Scene scene;
  layout(offset = 8) uint16_t depth_texture;
  layout(offset = 10) uint16_t normal_texture;
  layout(offset = 12) uint16_t environment_uv_image;
  layout(offset = 14) uint16_t environment_numerator_image;
  layout(offset = 16) uint16_t transmittance_texture;
  layout(offset = 18) uint16_t sky_view_lut;
  // offset 20: 4 bytes unused (buffer references need 8-byte alignment).
  layout(offset = 24) Direct_queue queue;
  layout(offset = 32) Rt_block_shape_grid rt_block_shape_grid;
  layout(offset = 40) Rt_entities entities;
  layout(offset = 48) Rt_entity_head_grid entity_head_grid;
  layout(offset = 56) Rt_entity_nodes entity_nodes;
  layout(offset = 64) Rt_block_material_grid rt_block_material_grid;
  layout(offset = 72) Rt_entity_mask_grid entity_mask_grid;
  // Used only by the persistent sky trace; the sun trace takes one ray
  // per invocation and ignores it.
  layout(offset = 80) Direct_ray_cursor cursor;
} push_constants;

#endif
