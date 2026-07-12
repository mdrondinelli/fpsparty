#version 450

#include "mesh.glsl"

layout(location = 0) out vec3 out_world_normal;
layout(location = 1) out vec3 out_albedo;
layout(location = 2) out vec4 out_current_clip;
layout(location = 3) out vec4 out_previous_clip;

void main() {
  const Vertex vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
  const vec4 model_position =
    vec4(vertex.position[0], vertex.position[1], vertex.position[2], 1.0);
  const vec3 world_position = push_constants.model_matrix * model_position;
  const vec3 previous_world_position =
    push_constants.previous_model_matrix * model_position;
  const vec3 model_normal =
    vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
  const vec3 world_normal = mat3(push_constants.model_matrix) * model_normal;
  out_world_normal = world_normal;
  out_albedo = vec3(vertex.color[0], vertex.color[1], vertex.color[2]);
  gl_Position = push_constants.scene.view_projection_matrix *
                vec4(world_position, 1.0);
  out_current_clip = gl_Position;
  out_previous_clip = push_constants.scene.previous_view_projection_matrix *
                      vec4(previous_world_position, 1.0);
}
