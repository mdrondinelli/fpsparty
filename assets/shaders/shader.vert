#version 450

#include "mesh.glsl"

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_world_normal;
layout(location = 2) out vec3 out_albedo;

void main() {
  const Vertex vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
  const vec4 model_position =
    vec4(vertex.position[0], vertex.position[1], vertex.position[2], 1.0);
  const vec4 world_position = push_constants.model_matrix * model_position;
  const vec3 model_normal =
    vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
  const vec3 world_normal = mat3(push_constants.model_matrix) * model_normal;
  out_world_position = world_position.xyz;
  out_world_normal = world_normal;
  out_albedo = vec3(vertex.color[0], vertex.color[1], vertex.color[2]);
  gl_Position = push_constants.scene.view_projection_matrix * world_position;
}
