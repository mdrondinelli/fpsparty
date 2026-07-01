#ifndef FPSPARTY_GRID_GLSL
#define FPSPARTY_GRID_GLSL

#include "descriptors.glsl"
#include "scene.glsl"

struct Vertex {
  float position[3];
  float texcoord[2];
  uint texture_index;
};

layout(std430, buffer_reference, buffer_reference_align = 4)
readonly buffer Vertex_buffer {
  Vertex vertices[];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
readonly buffer Texture_buffer {
  uint textures[];
};

layout(push_constant) uniform Push_constants {
  layout(offset = 0) Scene scene;
  layout(offset = 8) Vertex_buffer vertex_buffer;
  layout(offset = 16) Texture_buffer texture_buffer;
  layout(offset = 24) float normal_x;
  layout(offset = 28) float normal_y;
  layout(offset = 32) float normal_z;
  layout(offset = 36) uint transmittance_texture;
} push_constants;

#endif
