#version 450

#include "grid.glsl"

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) flat in uint in_texture_index;

layout(location = 0) out vec4 out_color;

void main() {
  const uint texture_index = nonuniformEXT(in_texture_index);
  const vec3 base_color = texture(
    sampler2D(sampled_images[texture_index], FPSPARTY_SAMPLER_NEAREST),
    in_texcoord).rgb;
  const vec3 n = push_constants.normal.xyz;
  if (push_constants.sun_direction.y >= 0.0f) {
    const float n_dot_l = max(dot(n, push_constants.sun_direction), 0.0f); 
    const vec3 E = vec3(1000.0f);
    const vec3 L = base_color * E * n_dot_l / 3.14159f;
    out_color = vec4(L, 1.0f);
  } else {
    out_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
  }
}
