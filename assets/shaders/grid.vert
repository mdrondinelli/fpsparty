#version 450

#include "grid.glsl"

layout(location = 0) out vec3 out_world_space_position;

void main() {
  Vertex vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
  vec3 position =
    vec3(vertex.position[0], vertex.position[1], vertex.position[2]);
  gl_Position = 
    push_constants.view_projection_matrix * 
    vec4(position + push_constants.normal.xyz * push_constants.normal.w, 1.0);
  out_world_space_position = position;
}
