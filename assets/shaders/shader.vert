#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 out_color;

layout(push_constant) uniform Push_constants {
  mat4 model_view_projection_matrix;
} push_constants;

void main() {
  gl_Position = push_constants.model_view_projection_matrix * vec4(in_position, 1.0);
  out_color = in_color;
}
