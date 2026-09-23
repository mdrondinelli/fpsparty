#ifndef FPSPARTY_DIRECT_COMMON_GLSL
#define FPSPARTY_DIRECT_COMMON_GLSL

#include "descriptors.glsl"
#include "gbuffer.glsl"
#include "intersectors.glsl"
#include "numbers.glsl"
#include "scene.glsl"
#include "atmosphere/atmosphere.glsl"

const float sun_angular_diameter = 0.0093;
const float sun_angular_radius = sun_angular_diameter / 2.0;
const float cos_sun_angular_radius = cos(sun_angular_radius);
const float sun_solid_angle = 2.0 * pi * (1.0 - cos_sun_angular_radius);

// Sample queue identity determines which distribution generates the ray.
struct Direct_sample {
  uint pixel; // Flat row-major pixel index.
  uint uv; // packUnorm2x16 random variables.
  float p_cone; // Environment mixture weight, retained for MIS evaluation.
};

// A sample is an (emitter, direction) pair, and direct irradiance is one
// integral over that space. Two techniques produce pairs, one sample each:
//
//   environment  names the distant environment, then draws w_i from its
//                own two-lobe mixture
//                  p_env = p_cone * I_disc / sun_solid_angle
//                        + (1 - p_cone) * cos_theta / pi
//   brdf         draws w_i from cos_theta / pi and traces; the first
//                emitter hit supplies the index
//
// Balance MIS over the two gives each sample the summed density at the
// pair it landed on: p_env + p_brdf. No sum over emitters appears because
// neither technique forgets which emitter it named -- other emitters have
// density at other pairs, not this one.
//
// The environment is one emitter, not two. Sun and sky are not separable
// by direction (the sky's radiance inside the solar disc is in-scatter,
// which the sun does not occlude), so they are lobes of its sampler
// rather than emitters of their own, and every ray evaluates sky
// everywhere plus sunlight inside the disc. Making p_cone per-pixel is
// what that buys: mass follows the sun only where the sun can be seen.
//
// P(emitter) is part of the pair density, not a sample count. It is 1 for
// the environment; nothing names an emissive surface by index yet, so
// there P * p is 0 -- see direct_emitted_contribution.
bool direct_in_sun_disc(vec3 w_i, vec3 sun_direction) {
  return dot(w_i, sun_direction) >= cos_sun_angular_radius;
}

// Summed density of both techniques at a pair naming the environment.
float direct_environment_mis_density(
    float p_cone, float cos_theta, bool in_sun_disc) {
  const float cone_density = in_sun_disc ? p_cone / sun_solid_angle : 0.0;
  return cone_density + (2.0 - p_cone) * cos_theta / pi;
}

// Primary surface reconstructed from the G-buffer, in world space.
struct Direct_shading_point {
  vec3 position;
  vec3 normal;
  // Offset along the normal to avoid self-intersection.
  vec3 ray_origin;
};

// A sample's pixel field is a flat row-major index into the frame.
ivec2 direct_sample_pixel(
    Direct_sample light_sample, ivec2 size) {
  return ivec2(
    int(light_sample.pixel % uint(size.x)),
    int(light_sample.pixel / uint(size.x)));
}

Direct_shading_point direct_shading_point(
    Scene scene,
    uint depth_texture,
    uint normal_texture,
    ivec2 pixel,
    ivec2 size) {
  const vec2 ndc = (vec2(pixel) + 0.5) / vec2(size) * 2.0 - 1.0;
  const vec3 view_ray_origin = scene.camera_basis[3].xyz;
  const vec3 view_ray_direction =
    vec3(-ndc.x * scene.zoom.x, -ndc.y * scene.zoom.y, 1.0);
  const Gbuffer_sample g = gbuffer_decode(
    texelFetch(sampled_images[depth_texture], pixel, 0).x,
    scene.z_near,
    texelFetch(sampled_images[normal_texture], pixel, 0).xy,
    0.0);
  const vec3 position =
    view_ray_origin +
    mat3(scene.camera_basis) * (view_ray_direction * g.linear_depth);
  return Direct_shading_point(
    position, g.normal, offset_ray_origin(position, g.normal));
}

layout(scalar, buffer_reference, buffer_reference_align = 4)
restrict buffer Direct_samples {
  uint count;
  Direct_sample samples[];
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

// Visibility-weighted environment irradiance contribution after MIS.
vec3 direct_environment_contribution(
    Scene scene,
    vec3 n,
    float shading_altitude,
    vec3 w_i,
    float visibility,
    float p_cone,
    uint transmittance_texture,
    uint sky_view_lut) {
  const float cos_theta = max(dot(n, w_i), 0.0);
  if (cos_theta <= 0.0) {
    return vec3(0.0);
  }
  const bool in_sun_disc =
    direct_in_sun_disc(w_i, scene.sun_direction);
  vec3 integrand = eval_incident_irradiance_sky(w_i, n, scene, sky_view_lut);
  if (in_sun_disc) {
    const vec3 transmittance = transmittance_along_ray(
      transmittance_texture,
      vec3(0.0, r_ground + shading_altitude, 0.0),
      w_i);
    integrand +=
      eval_incident_irradiance_sun(w_i, n, scene.sun_irradiance, transmittance);
  }
  return visibility * integrand /
    direct_environment_mis_density(p_cone, cos_theta, in_sun_disc);
}

// The (surface, w_i) pair, which only the BRDF technique can produce:
// no sampler names an emissive surface by index yet, so P * p is 0 and
// the denominator is p_brdf alone, whose cosine cancels the integrand's.
// Giving surfaces a sampler puts P * p back and this stops being a
// special case.
vec3 direct_emitted_contribution(
    vec3 emissivity, float cos_theta) {
  if (cos_theta <= 0.0) {
    return vec3(0.0);
  }
  return emissivity * pi;
}

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
