#ifndef FPSPARTY_MESH_GLSL
#define FPSPARTY_MESH_GLSL

#include "scene.glsl"

struct Vertex {
  float position[3];
  float normal[3];
  float color[3];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
readonly buffer Vertex_buffer {
  Vertex vertices[];
};

layout(push_constant) uniform Push_constants {
  layout(offset = 0) Scene scene;
  layout(offset = 8) Vertex_buffer vertex_buffer;
  layout(offset = 16) mat4 model_matrix;
  layout(offset = 80) uint transmittance_texture;
} push_constants;

#endif
