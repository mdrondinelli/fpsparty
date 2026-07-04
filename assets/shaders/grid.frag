#version 450

#include "grid.glsl"
#include "octahedral.glsl"

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) flat in uint in_texture;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec2 out_normal;

void main() {
  const vec3 base_color =
    FPSPARTY_SAMPLE(nonuniformEXT(in_texture), in_texcoord).rgb;
  const vec3 n = vec3(
    push_constants.normal_x,
    push_constants.normal_y,
    push_constants.normal_z);
  out_albedo = vec4(base_color, 1.0);
  out_normal = oct_encode(n);
}
