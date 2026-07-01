#version 450

#include "../descriptors.glsl"
#include "../numbers.glsl"
#include "atmosphere.glsl"

layout(push_constant) uniform Push_constants {
  layout(offset = 0) mat4 camera_basis;
  layout(offset = 64) vec3 sun_direction;
  layout(offset = 80) vec3 sun_irradiance;
  layout(offset = 92) uint transmittance_lut;
  layout(offset = 96) vec2 zoom;
} push_constants;

layout(location = 0) in vec2 in_ndc;

layout(location = 0) out vec4 out_color;

// ro must be inside the atmosphere
float ray_atmosphere(vec3 ro, vec3 rd) {
  const float b = dot(ro, rd);
  const float c = dot(ro, ro) - r_atmosphere * r_atmosphere;
  const float h = b * b - c;
  const float t = -b + sqrt(max(h, 0.0));
  return t;
}

// ro must be outside the planet
float ray_planet(vec3 ro, vec3 rd) {
  const float r_min = r_ground - 400.0;
  const float b = dot(ro, rd);
  const float c = dot(ro, ro) - r_min * r_min;
  const float h = b * b - c;
  if (h < 0.0) {
    return -1.0 / 0.0;
  }
  return -b - sqrt(h);
}

vec3 transmittance_along_ray(vec3 ro, vec3 rd) {
  const float h = altitude(ro);
  const float cos_zenith = dot(normalize(ro), rd);
  const vec2 lut_texcoord = pack_transmittance_lut_params(h, cos_zenith);
  return FPSPARTY_SAMPLE(
      push_constants.transmittance_lut,
      LINEAR_CLAMP,
      lut_texcoord).rgb;
}

vec3 transmittance_ratio(vec3 numerator, vec3 denominator) {
  return numerator / max(denominator, vec3(1.0e-6));
}

vec3 integrate_in_scattering(vec3 x_0, vec3 x_1, vec3 d) {
  const vec3 transmittance_numer = transmittance_along_ray(x_0, d);
  const int step_count = 64;
  const float step_size = length(x_0 - x_1) / step_count;
  vec3 radiance = vec3(0.0);
  for (int i = 0; i < step_count; ++i) {
    const vec3 x_i = mix(x_0, x_1, (i + 0.5) / step_count);
    const float h = altitude(x_i);
    const float cos_theta = dot(d, push_constants.sun_direction);
    const vec3 rayleigh_scattering_coeff =
      rayleigh_scattering * rayleigh_phase(cos_theta) * rayleigh_density(h);
    const float mie_scattering_coeff =
      mie_scattering * mie_phase(cos_theta) * mie_density(h);
    const vec3 irradiance =
      transmittance_along_ray(x_i, push_constants.sun_direction) *
      push_constants.sun_irradiance;
    const vec3 transmittance_denom = transmittance_along_ray(x_i, d);
    const vec3 transmittance_to_sample = 
      transmittance_ratio(transmittance_numer, transmittance_denom);
    radiance +=
      (rayleigh_scattering_coeff + vec3(mie_scattering_coeff)) * irradiance *
      transmittance_to_sample * step_size;
  }
  return radiance;
}

void main() {
  const vec3 camera_position = push_constants.camera_basis[3].xyz;
  const vec3 ro = vec3(0.0, r_ground + camera_position.y, 0.0);
  const vec3 rd =
    normalize(
      mat3(push_constants.camera_basis) *
      vec3(vec2(-in_ndc.x, -in_ndc.y) * push_constants.zoom, 1.0));
  const float t_atmosphere = ray_atmosphere(ro, rd);
  const float t_planet = ray_planet(ro, rd);
  const bool hit_planet = t_planet >= 0.0;
  const float t = hit_planet ? t_planet : t_atmosphere;
  const vec3 p = ro + t * rd;
  const vec3 x_0 = hit_planet ? p : ro;
  const vec3 x_1 = hit_planet ? ro : p;
  const vec3 d = hit_planet ? -rd : rd;
  const vec3 in_scattering = integrate_in_scattering(x_0, x_1, d);
  vec3 radiance = in_scattering;
  if (hit_planet) {
    const vec3 n = normalize(p);
    const float n_dot_l = max(dot(n, push_constants.sun_direction), 0.0);
    const vec3 arriving_irradiance =
      transmittance_along_ray(p, push_constants.sun_direction) *
      push_constants.sun_irradiance;
    const vec3 reflected_radiance =
      planet_albedo / pi * arriving_irradiance * n_dot_l;
    radiance +=
      transmittance_ratio(
        transmittance_along_ray(p, -rd),
        transmittance_along_ray(ro, -rd)) *
      reflected_radiance;
  }
  out_color = vec4(radiance, 1.0);
}
