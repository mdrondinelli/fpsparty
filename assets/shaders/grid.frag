#version 450

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform Push_constants {
  layout(offset = 64) vec4 normal;
} push_constants;

void main() {
  out_color = vec4(abs(push_constants.normal.rgb) * (gl_FrontFacing ? 1.0f : 0.5f), 1.0f);
}
