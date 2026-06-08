#extension GL_EXT_buffer_reference : require

#include "descriptors.glsl"

struct Vertex {
  float position[3];
};

layout(std430, buffer_reference, buffer_reference_align = 4)
readonly buffer Vertex_buffer {
  Vertex vertices[];
};

layout(push_constant) uniform Push_constants {
  layout(offset = 0) mat4 view_projection_matrix;
  layout(offset = 64) vec4 normal;
  layout(offset = 80) Vertex_buffer vertex_buffer;
  layout(offset = 88) uint texture_index;
} push_constants;
