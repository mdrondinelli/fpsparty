#version 450

#include "grid.glsl"

layout(location = 0) in vec3 in_world_space_position;
layout(location = 1) flat in uint in_texture_index;

layout(location = 0) out vec4 out_color;

void main() {
  const uint texture_index = nonuniformEXT(
      push_constants.texture_indices[in_texture_index]);
  const vec3 normal = push_constants.normal.xyz;
  vec2 texture_coordinates;
  if (abs(normal.x) > 0.5f) {
    if (normal.x > 0.0f) {
      texture_coordinates = vec2(
        1.0f - in_world_space_position.z,
        1.0f - in_world_space_position.y);
    } else {
      texture_coordinates = vec2(
        in_world_space_position.z,
        1.0f - in_world_space_position.y);
    }
  } else if (abs(normal.y) > 0.5f) {
    if (normal.y > 0.0f) {
      texture_coordinates = vec2(
          1.0f - in_world_space_position.x,
          1.0f - in_world_space_position.z);
    } else {
      texture_coordinates = vec2(
          in_world_space_position.x,
          1.0f - in_world_space_position.z);
    }
  } else {
    if (normal.z > 0.0f) {
      texture_coordinates = vec2(
        in_world_space_position.x,
        1.0f - in_world_space_position.y);
    } else {
      texture_coordinates = vec2(
        1.0f - in_world_space_position.x,
        1.0f - in_world_space_position.y);
    }
  }
  const ivec2 tex_size = textureSize(
    sampler2D(sampled_images[texture_index], FPSPARTY_SAMPLER_LINEAR), 0);
  const vec3 base_color = texelFetch(
    sampler2D(sampled_images[texture_index], FPSPARTY_SAMPLER_LINEAR),
    ivec2(fract(texture_coordinates) * tex_size),
    0).rgb;
  out_color = vec4(base_color, 1.0f);
}
