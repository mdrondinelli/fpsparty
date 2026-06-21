#version 450

#include "grid.glsl"

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) flat in uint in_texture_index;

layout(location = 0) out vec4 out_color;

void main() {
  const uint texture_index = nonuniformEXT(in_texture_index);
  const vec3 normal = push_constants.normal.xyz;
  const vec3 base_color = texture(
    sampler2D(sampled_images[texture_index], FPSPARTY_SAMPLER_NEAREST),
    in_texcoord).rgb;
  const float light = normal.y * 0.5f + 0.5f;
  out_color = vec4(base_color * (light + 0.1f), 1.0f);
}
