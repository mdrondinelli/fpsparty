#ifndef FPSPARTY_MESH_GLSL
#define FPSPARTY_MESH_GLSL

#include "scene.glsl"

struct Vertex {
  float position[3];
  float normal[3];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
readonly buffer Vertex_buffer {
  Vertex vertices[];
};

layout(push_constant) uniform Push_constants {
  layout(offset = 0) Scene scene;
  layout(offset = 8) Vertex_buffer vertex_buffer;
  layout(offset = 16, row_major) mat4x3 model_matrix;
  layout(offset = 64, row_major) mat4x3 previous_model_matrix;
} push_constants;

#endif
