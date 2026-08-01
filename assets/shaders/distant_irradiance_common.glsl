#ifndef FPSPARTY_DISTANT_IRRADIANCE_COMMON_GLSL
#define FPSPARTY_DISTANT_IRRADIANCE_COMMON_GLSL

#include "descriptors.glsl"
#include "numbers.glsl"
#include "octahedral.glsl"
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
      sampled_images[sky_view_lut],
      pack_sky_view_lut_params(
        longitude(w_i), scene.camera_basis[3][1], zenith(w_i)),
      0.0).rgb;
  return L_i * max(dot(n, w_i), 0.0);
}

const float distant_irradiance_history_depth_reject_ratio = 0.03;

const float distant_irradiance_history_normal_reject_cos = 0.9;

// Applies temporal accumulation to the distant irradiance signal. Shared by
// distant_irradiance_trace_sun.comp and distant_irradiance_trace_sky.comp.
//
// History is manually 4-tap bilinear rather than hardware-filtered: hardware
// filtering would silently blend in a disoccluded neighbor's rejected texel
// at the same weight as valid ones. Instead each of the 4 nearest texels is
// depth- and normal-tested individually; a rejected tap's bilinear weight is
// dropped and the remaining taps' weights renormalized to sum to 1, so a
// disocclusion only removes/redistributes weight rather than corrupting it.
//
// @param current the new distant irradiance sample
// @param n the normal at the sample's pixel
// @param pixel the sample's pixel coordinates
// @param size the extent of the distant irradiance image
// @param z_near the camera's near plane distance, for linearizing depth
// @param motion_vector_texture this frame's motion vector texture
// @param previous_depth_texture the previous frame's depth texture
// @param previous_normal_texture the previous frame's normal texture
// @param previous_distant_irradiance_texture the previous frame's
//   accumulated distant irradiance texture
// @param history_valid whether the previous frame's textures hold valid
//   history (0 forces current to be returned unchanged)
// @return current unmixed if there's no usable history (first frame /
//   resize, off-screen reprojection, or all 4 history taps rejected);
//   otherwise mix(history, current, 0.2)
vec3 apply_distant_irradiance_history(
    vec3 current,
    vec3 n,
    ivec2 pixel,
    ivec2 size,
    float z_near,
    uint motion_vector_texture,
    uint previous_depth_texture,
    uint previous_normal_texture,
    uint previous_distant_irradiance_texture,
    uint history_valid) {
  if (history_valid == 0u) {
    return current;
  }
  const vec2 uv = (vec2(pixel) + 0.5) / vec2(size);
  const vec3 motion_and_previous_depth =
    texelFetch(sampled_images[motion_vector_texture], pixel, 0).rgb;
  const vec2 previous_uv = uv + motion_and_previous_depth.xy;
  if (any(lessThan(previous_uv, vec2(0.0))) ||
      any(greaterThan(previous_uv, vec2(1.0)))) {
    return current;
  }
  const float previous_linear_depth = z_near / motion_and_previous_depth.z;

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
  vec3 history_sum = vec3(0.0);
  for (int i = 0; i < 4; ++i) {
    const ivec2 coord = clamp(base + offsets[i], ivec2(0), max_coord);
    const float history_depth =
      texelFetch(sampled_images[previous_depth_texture], coord, 0).r;
    const float history_linear_depth = z_near / history_depth;
    if (abs(previous_linear_depth - history_linear_depth) >
        distant_irradiance_history_depth_reject_ratio * history_linear_depth) {
      continue;
    }
    const vec3 history_normal = oct_decode(
      texelFetch(sampled_images[previous_normal_texture], coord, 0).rg);
    if (dot(n, history_normal) < distant_irradiance_history_normal_reject_cos) {
      continue;
    }
    const vec3 history_color =
      texelFetch(
        sampled_images[previous_distant_irradiance_texture], coord, 0).rgb;
    weight_sum += weights[i];
    history_sum += weights[i] * history_color;
  }
  if (weight_sum <= 0.0) {
    return current;
  }
  const vec3 history = history_sum / weight_sum;
  return mix(history, current, 0.2);
}

#endif
