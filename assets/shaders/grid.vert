#version 450

#include "grid.glsl"

layout(location = 0) out vec2 out_texcoord;
layout(location = 1) flat out uint out_texture;
layout(location = 2) out vec4 out_current_clip;
layout(location = 3) out vec4 out_previous_clip;
layout(location = 4) flat out float out_emissivity_scale;

void main() {
  Vertex vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
  const vec3 position =
    vec3(vertex.position[0], vertex.position[1], vertex.position[2]);
  gl_Position =
    push_constants.scene.view_projection_matrix * vec4(position, 1.0);
  out_current_clip = gl_Position;
  out_previous_clip =
    push_constants.scene.previous_view_projection_matrix * vec4(position, 1.0);
  if (vertex.texture_index == 3) {
    vertex.texcoord[1] += push_constants.scene.animation_time * 0.5;
  }
  out_texcoord = vec2(vertex.texcoord[0], vertex.texcoord[1]);
  out_texture = push_constants.texture_buffer.textures[vertex.texture_index];
  out_emissivity_scale = vertex.emissivity_scale;
}
