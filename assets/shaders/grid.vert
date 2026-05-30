#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out float depth;

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
} push_constants;

void main() {
  Vertex vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
  vec3 position =
    vec3(vertex.position[0], vertex.position[1], vertex.position[2]);
  gl_Position = 
    push_constants.view_projection_matrix * 
    vec4(position + push_constants.normal.xyz * push_constants.normal.w, 1.0);
  depth = gl_Position.w;
}
