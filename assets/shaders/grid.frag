#version 450

#include "grid.glsl"

layout(location = 0) in vec3 in_world_space_position;
layout(location = 1) in float in_view_space_depth;

layout(location = 0) out vec4 out_color;

void main() {
  const uint texture_index = nonuniformEXT(push_constants.texture_index);
  const vec3 normal = push_constants.normal.xyz;
  vec2 texture_coordinates;
  if (abs(normal.x) > 0.5f) {
    if (normal.x > 0.0f) {
      texture_coordinates = vec2(
        1.0f - in_world_space_position.z,
        in_world_space_position.y);
    } else {
      texture_coordinates = vec2(
        in_world_space_position.z,
        in_world_space_position.y);
    }
  } else if (abs(normal.y) > 0.5f) {
    texture_coordinates = vec2(1.0f, 1.0f) - in_world_space_position.xz;
  } else {
    if (normal.z > 0.0f) {
      texture_coordinates = in_world_space_position.xy;
    } else {
      texture_coordinates = vec2(
        1.0f - in_world_space_position.x,
        in_world_space_position.y);
    }
  }
  const ivec2 tex_size = textureSize(
    sampler2D(sampled_images[texture_index], FPSPARTY_SAMPLER_LINEAR), 0);
  const vec3 base_color = texelFetch(
    sampler2D(sampled_images[texture_index], FPSPARTY_SAMPLER_LINEAR),
    ivec2(fract(texture_coordinates) * tex_size),
    0).rgb;
  const vec3 fog_color = vec3(0.4196f, 0.6196f, 0.7451f);
  out_color = vec4(
    mix(
      base_color,
      fog_color,
      1.0f - pow(1.0f - 0.125f, in_view_space_depth)
    ),
    1.0f);
}
