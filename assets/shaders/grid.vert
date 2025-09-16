#version 450

layout(location = 0) in vec3 in_position;

layout(location = 0) out float depth;

layout(push_constant) uniform Push_constants {
  layout(offset = 0) mat4 view_projection_matrix;
  layout(offset = 64) vec4 normal;
} push_constants;

void main() {
  gl_Position = 
    push_constants.view_projection_matrix * 
    vec4(in_position + push_constants.normal.xyz * push_constants.normal.w, 1.0);
  depth = gl_Position.w;
}
