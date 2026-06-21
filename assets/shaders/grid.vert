#version 450

#include "grid.glsl"

layout(location = 0) out vec2 out_texcoord;
layout(location = 1) flat out uint out_texture_index;

void main() {
  Vertex vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
  vec3 position =
    vec3(vertex.position[0], vertex.position[1], vertex.position[2]);
  gl_Position = 
    push_constants.view_projection_matrix * 
    vec4(position + push_constants.normal.xyz * push_constants.normal.w, 1.0);
  if (vertex.texture_index == 3) {
    vertex.texcoord[1] += push_constants.time * 0.5;
  }
  out_texcoord = vec2(vertex.texcoord[0], vertex.texcoord[1]);
  out_texture_index =
    push_constants.texture_index_buffer.indices[vertex.texture_index];
}
