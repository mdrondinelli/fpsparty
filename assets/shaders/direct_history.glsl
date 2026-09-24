#ifndef FPSPARTY_DIRECT_HISTORY_GLSL
#define FPSPARTY_DIRECT_HISTORY_GLSL

#include "descriptors.glsl"
#include "gbuffer.glsl"

const float direct_history_depth_reject_ratio = 0.03;

const float direct_history_normal_reject_cos = 0.9659;

// History length is stored in irradiance alpha; blending uses 1 / length.
const float max_direct_history_length = 30.0;

// Bilinear reprojection rejects each tap by depth and normal, then
// redistributes its weight among surviving taps.
struct Direct_history {
  vec3 color;
  vec2 luminance_moments;
  float history_length;
};

Direct_history apply_direct_history(
    vec3 current_color,
    vec2 current_luminance_moments,
    vec3 n,
    ivec2 pixel,
    ivec2 size,
    uint motion_vector_texture,
    uint previous_depth_texture,
    uint previous_normal_texture,
    float z_near,
    uint previous_direct_irradiance_texture,
    uint previous_direct_luminance_texture,
    uint history_valid) {
  if (history_valid == 0u) {
    return Direct_history(
      current_color, current_luminance_moments, 1.0);
  }
  const vec2 uv = (vec2(pixel) + 0.5) / vec2(size);
  const vec3 motion_and_previous_depth =
    texelFetch(sampled_images[motion_vector_texture], pixel, 0).rgb;
  const vec2 previous_uv = uv + motion_and_previous_depth.xy;
  if (any(lessThan(previous_uv, vec2(0.0))) ||
      any(greaterThan(previous_uv, vec2(1.0)))) {
    return Direct_history(
      current_color, current_luminance_moments, 1.0);
  }
  // motion_and_previous_depth.z is already linear -- see motion_vector().
  const float previous_linear_depth = motion_and_previous_depth.z;

  const vec2 texel = previous_uv * vec2(size) - 0.5;
  const ivec2 base = ivec2(floor(texel));
  const vec2 f = texel - vec2(base);
  const ivec2 max_coord = size - ivec2(1);
  const ivec2 offsets[4] =
    ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));
  const float weights[4] = float[4](
    (1.0 - f.x) * (1.0 - f.y),
    f.x * (1.0 - f.y),
    (1.0 - f.x) * f.y,
    f.x * f.y);

  float weight_sum = 0.0;
  vec3 color_sum = vec3(0.0);
  float history_length_sum = 0.0;
  vec2 luminance_sum = vec2(0.0);
  for (int i = 0; i < 4; ++i) {
    const ivec2 coord = clamp(base + offsets[i], ivec2(0), max_coord);
    const Gbuffer_sample history = gbuffer_decode(
      texelFetch(sampled_images[previous_depth_texture], coord, 0).x,
      z_near,
      texelFetch(sampled_images[previous_normal_texture], coord, 0).xy,
      0.0);
    if (abs(previous_linear_depth - history.linear_depth) >
        direct_history_depth_reject_ratio * previous_linear_depth) {
      continue;
    }
    if (dot(n, history.normal) < direct_history_normal_reject_cos) {
      continue;
    }
    weight_sum += weights[i];
    const vec4 previous_color =
      texelFetch(
        sampled_images[previous_direct_irradiance_texture], coord, 0);
    color_sum += weights[i] * previous_color.rgb;
    history_length_sum += weights[i] * previous_color.a;
    luminance_sum +=
      weights[i] *
      texelFetch(
        sampled_images[previous_direct_luminance_texture],
        coord,
        0).rg;
  }
  if (weight_sum <= 0.0) {
    return Direct_history(
      current_color, current_luminance_moments, 1.0);
  }
  const float history_length = min(
    history_length_sum / weight_sum + 1.0,
    max_direct_history_length);
  const float alpha = 1.0 / history_length;
  return Direct_history(
    mix(color_sum / weight_sum, current_color, alpha),
    mix(luminance_sum / weight_sum, current_luminance_moments, alpha),
    history_length);
}

#endif
