#ifndef FPSPARTY_DISTANT_IRRADIANCE_COMMON_GLSL
#define FPSPARTY_DISTANT_IRRADIANCE_COMMON_GLSL

#include "descriptors.glsl"
#include "gbuffer.glsl"
#include "numbers.glsl"
#include "scene.glsl"
#include "atmosphere/atmosphere.glsl"

const float sun_angular_diameter = 0.0093;
const float sun_angular_radius = sun_angular_diameter / 2.0;
const float cos_sun_angular_radius = cos(sun_angular_radius);
const float sun_solid_angle = 2.0 * pi * (1.0 - cos_sun_angular_radius);

// Represents either a sun or sky sample.
struct Distant_light_sample {
  // The pixel index of the sample.
  uint pixel;
  // The random variables generated for the sample, packed via packUnorm2x16.
  uint uv;
};

layout(scalar, buffer_reference, buffer_reference_align = 4)
restrict buffer Distant_light_samples {
  uint count;
  Distant_light_sample samples[];
};

vec3 eval_incident_irradiance_sun(
    vec3 w_i, vec3 n, vec3 integrated_irradiance, vec3 transmittance) {
  const vec3 L_i = integrated_irradiance / sun_solid_angle * transmittance;
  return L_i * max(dot(n, w_i), 0.0);
}

vec3 eval_incident_irradiance_sky(vec3 w_i, vec3 n, Scene scene, uint sky_view_lut) {
  const vec3 L_i =
    textureLod(
      sampler2D(sampled_images[sky_view_lut], SAMPLER_LAT_LONG),
      pack_sky_view_lut_params(
        longitude(w_i), scene.camera_basis[3][1], zenith(w_i)),
      0.0).rgb;
  return L_i * max(dot(n, w_i), 0.0);
}

const float distant_irradiance_history_depth_reject_ratio = 0.03;

const float distant_irradiance_history_normal_reject_cos = 0.9659;

// History-length counter (frames survived reprojection, capped) drives the
// blend weight -- alpha = 1 / history_length -- instead of a fixed
// constant, so freshly-established history isn't over-blended and
// long-lived history converges to a lower noise floor. Stored in the
// distant irradiance color texture's otherwise-unused alpha channel.
const float max_distant_irradiance_history_length = 30.0;

// Applies temporal accumulation to the distant irradiance color and
// luminance moments (R = luminance, G = luminance^2, for a future variance
// estimate). Manual 4-tap bilinear (not hardware-filtered) so each tap can
// be depth/normal-rejected individually, with its weight redistributed
// among the survivors rather than corrupting the blend.
struct Distant_irradiance_history {
  vec3 color;
  vec2 luminance_moments;
  float history_length;
};

Distant_irradiance_history apply_distant_irradiance_history(
    vec3 current_color,
    vec2 current_luminance_moments,
    vec3 n,
    ivec2 pixel,
    ivec2 size,
    uint motion_vector_texture,
    uint previous_depth_normal_texture,
    uint previous_distant_irradiance_texture,
    uint previous_distant_irradiance_luminance_texture,
    uint history_valid) {
  if (history_valid == 0u) {
    return Distant_irradiance_history(
      current_color, current_luminance_moments, 1.0);
  }
  const vec2 uv = (vec2(pixel) + 0.5) / vec2(size);
  const vec3 motion_and_previous_depth =
    texelFetch(sampled_images[motion_vector_texture], pixel, 0).rgb;
  const vec2 previous_uv = uv + motion_and_previous_depth.xy;
  if (any(lessThan(previous_uv, vec2(0.0))) ||
      any(greaterThan(previous_uv, vec2(1.0)))) {
    return Distant_irradiance_history(
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
      texelFetch(sampled_images[previous_depth_normal_texture], coord, 0));
    if (abs(previous_linear_depth - history.linear_depth) >
        distant_irradiance_history_depth_reject_ratio * history.linear_depth) {
      continue;
    }
    if (dot(n, history.normal) < distant_irradiance_history_normal_reject_cos) {
      continue;
    }
    weight_sum += weights[i];
    const vec4 previous_color =
      texelFetch(
        sampled_images[previous_distant_irradiance_texture], coord, 0);
    color_sum += weights[i] * previous_color.rgb;
    history_length_sum += weights[i] * previous_color.a;
    luminance_sum +=
      weights[i] *
      texelFetch(
        sampled_images[previous_distant_irradiance_luminance_texture],
        coord,
        0).rg;
  }
  if (weight_sum <= 0.0) {
    return Distant_irradiance_history(
      current_color, current_luminance_moments, 1.0);
  }
  const float history_length = min(
    history_length_sum / weight_sum + 1.0,
    max_distant_irradiance_history_length);
  const float alpha = 1.0 / history_length;
  return Distant_irradiance_history(
    mix(color_sum / weight_sum, current_color, alpha),
    mix(luminance_sum / weight_sum, current_luminance_moments, alpha),
    history_length);
}

#endif
