#version 450

#include "descriptors.glsl"

layout(push_constant) uniform Push_constants {
  layout(offset = 0) uint albedo_texture_index;
  layout(offset = 4) uint mask_texture_index;
  layout(offset = 8) uint depth_texture_index;
  layout(offset = 12) float z_near;
} push_constants;

layout(location = 0) out vec4 out_color;

void main() {
  const ivec2 pixel = ivec2(gl_FragCoord.xy);
  const uint albedo_texture_index =
    push_constants.albedo_texture_index;
  const uint mask_texture_index =
    push_constants.mask_texture_index;
  const uint depth_texture_index =
    push_constants.depth_texture_index;
  vec3 color = texelFetch(
    sampler2D(sampled_images[albedo_texture_index], FPSPARTY_SAMPLER_NEAREST),
    pixel,
    0).rgb;
  const float depth = texelFetch(
    sampler2D(sampled_images[depth_texture_index], FPSPARTY_SAMPLER_NEAREST),
    pixel,
    0).r;
  const float view_depth = push_constants.z_near / depth;
  const vec3 fog_color = vec3(0.4196f, 0.6196f, 0.7451f);
  /*
  color = mix(
    color,
    fog_color,
    1.0f - pow(15.0f / 16.0f, view_depth));
  */
  const float mask = texelFetch(
    sampler2D(sampled_images[mask_texture_index], FPSPARTY_SAMPLER_NEAREST),
    pixel,
    0).r;
  if (mask > 0.5f) {
    color = vec3(1.0f) - color;
  }
  out_color = vec4(color, 1.0f);
}
