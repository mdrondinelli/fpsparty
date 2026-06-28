#version 450

#include "descriptors.glsl"

layout(push_constant) uniform Push_constants {
  layout(offset = 0) uint albedo_texture_index;
  layout(offset = 4) uint mask_texture_index;
  layout(offset = 8) uint depth_texture_index;
  layout(offset = 12) float z_near;
  layout(offset = 16) uint frame_number;
} push_constants;

layout(location = 0) out vec4 out_color;

uvec3 pcg3d(uvec3 v) 
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y*v.z;
  v.y += v.z*v.x;
  v.z += v.x*v.y;
  v ^= v >> 16u;
  v.x += v.y*v.z;
  v.y += v.z*v.x;
  v.z += v.x*v.y;
  return v;
}

vec3 srgb_to_linear(vec3 srgb) {
  return pow(srgb, vec3(2.2f));
}

vec3 linear_to_srgb(vec3 linear) {
  return pow(linear, vec3(1.0f / 2.2f));
}

vec3 apply_noise(vec3 color, vec3 noise) {
  const float quantization = 1.0f / 256.0f;
  return srgb_to_linear(
    max(linear_to_srgb(color) + noise * quantization, vec3(0.0f)));
}

void main() {
  const ivec2 pixel = ivec2(gl_FragCoord.xy);
  const uvec3 pixel_hash =
    pcg3d(uvec3(uvec2(pixel), uint(push_constants.frame_number)));
  const vec3 pixel_noise = vec3(pixel_hash) / 4294967296.0f - 0.5;
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
  color *= 1.0f / 512.0f; // exposure 
  color /= color + vec3(1.0f); // tonemap
  color = apply_noise(color, pixel_noise);
  const float mask = texelFetch(
    sampler2D(sampled_images[mask_texture_index], FPSPARTY_SAMPLER_NEAREST),
    pixel,
    0).r;
  if (mask > 0.5f) {
    color = vec3(1.0f) - color;
  }
  out_color = vec4(color, 1.0f);
}
