#version 450

layout(location = 0) in float depth;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform Push_constants {
  layout(offset = 64) vec4 normal;
} push_constants;

void main() {
  vec3 base_color = abs(push_constants.normal.rgb) * (gl_FrontFacing ? 1.0f : 0.5f);
  vec3 fog_color = vec3(0.4196f, 0.6196f, 0.7451f);
  out_color = vec4(mix(base_color, fog_color, 1.0f - pow(1.0f - 0.125f, depth)), 1.0f);
}
