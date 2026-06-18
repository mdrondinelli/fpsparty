#version 450

#include "descriptors.glsl"

layout(push_constant) uniform Push_constants {
  layout(offset = 0) uint hdr_texture_index;
  layout(offset = 4) uint mask_texture_index;
} push_constants;

layout(location = 0) out vec4 out_color;

vec3 linear_to_srgb(vec3 color) {
  return mix(
    1.055f * pow(color, vec3(1.0f / 2.4f)) - vec3(0.055f),
    12.92f * color,
    lessThanEqual(color, vec3(0.0031308f)));
}

vec3 srgb_to_linear(vec3 color) {
  return mix(
    pow((color + vec3(0.055f)) / 1.055f, vec3(2.4f)),
    color / 12.92f,
    lessThanEqual(color, vec3(0.04045f)));
}

void main() {
  const ivec2 pixel = ivec2(gl_FragCoord.xy);
  const uint hdr_texture_index =
    push_constants.hdr_texture_index;
  const uint mask_texture_index =
    push_constants.mask_texture_index;
  vec3 color = texelFetch(
    sampler2D(sampled_images[hdr_texture_index], FPSPARTY_SAMPLER_NEAREST),
    pixel,
    0).rgb;
  color = color / (color + vec3(1.0f));
  const float mask = texelFetch(
    sampler2D(sampled_images[mask_texture_index], FPSPARTY_SAMPLER_NEAREST),
    pixel,
    0).r;
  if (mask > 0.5f) {
    //const vec3 srgb_color = linear_to_srgb(clamp(color, vec3(0.0f), vec3(1.0f)));
    //color = srgb_to_linear(vec3(1.0f) - srgb_color);
    color = vec3(1.0f) - color;
  }
  out_color = vec4(color, 1.0f);
}
