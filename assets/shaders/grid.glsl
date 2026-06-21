#extension GL_EXT_buffer_reference : require

#include "descriptors.glsl"

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
readonly buffer Texture_index_buffer {
  uint indices[];
};

layout(push_constant) uniform Push_constants {
  layout(offset = 0) mat4 view_projection_matrix;
  layout(offset = 64) vec4 normal;
  layout(offset = 80) Vertex_buffer vertex_buffer;
  layout(offset = 88) Texture_index_buffer texture_index_buffer;
  layout(offset = 96) float time;
} push_constants;
