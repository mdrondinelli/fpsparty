#ifndef FPSPARTY_DIRECT_TRACE_INTERFACE_GLSL
#define FPSPARTY_DIRECT_TRACE_INTERFACE_GLSL

#include "direct_common.glsl"
#include "rt.glsl"

// Shared by all trace shaders; offsets match Direct_trace_pass::execute.
layout(push_constant) uniform Push_constants {
  layout(offset = 0) Scene scene;
  layout(offset = 8) uint16_t depth_texture;
  layout(offset = 10) uint16_t direct_irradiance_image;
  layout(offset = 12) uint16_t transmittance_texture;
  layout(offset = 14) uint16_t sky_view_lut;
  layout(offset = 16) uint16_t normal_texture;
  // offset 18: 6 bytes unused (buffer references need 8-byte alignment).
  layout(offset = 24) Direct_samples samples;
  layout(offset = 32) Rt_block_shape_grid rt_block_shape_grid;
  layout(offset = 40) Rt_entities entities;
  layout(offset = 48) Rt_entity_head_grid entity_head_grid;
  layout(offset = 56) Rt_entity_nodes entity_nodes;
  layout(offset = 64) Rt_block_material_grid rt_block_material_grid;
  layout(offset = 72) Rt_entity_mask_grid entity_mask_grid;
} push_constants;

#endif
