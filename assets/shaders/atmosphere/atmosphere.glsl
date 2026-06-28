#ifndef FPSPARTY_ATMOSPHERE_ATMOSPHERE_GLSL
#define FPSPARTY_ATMOSPHERE_ATMOSPHERE_GLSL

#include "../numbers.glsl"

const float r_ground = 6360000.0;
const float r_atmosphere = 6460000.0;
const float min_altitude = -2400.0;
const float max_altitude = 100000.0;

const vec3 rayleigh_scattering = vec3(5.802e-6, 13.558e-6, 33.1e-6);

const float mie_scattering = 3.996e-6;
const float mie_absorption = 4.40e-6;
const float mie_extinction = mie_scattering + mie_absorption;

const vec3 ozone_absorption = vec3(0.650e-6, 1.881e-6, 0.085e-6);

const float planet_albedo = 0.3;

float altitude(vec3 position) {
  return length(position) - r_ground;
}

float rayleigh_phase(float cos_theta) {
  return 3.0 / (16.0 * pi) * (1.0 + cos_theta * cos_theta);
}

float mie_phase(float cos_theta) {
  const float g = 0.8;
  return 3.0 / (8.0 * pi) * (1.0 - g * g) * (1.0 + cos_theta * cos_theta) /
    ((2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * cos_theta, 1.5));
}

float rayleigh_density(float altitude) {
  return exp(-altitude / 8000.0);
}

float mie_density(float altitude) {
  return exp(-altitude / 1200.0);
}

float ozone_density(float altitude) {
  return max(0.0, 1.0 - abs(altitude - 25000.0) / 15000.0);
}

vec2 pack_transmittance_lut_params(float altitude, float cos_zenith) {
  const float x = (altitude - min_altitude) / (max_altitude - min_altitude);
  const float y = cos_zenith * 0.5 + 0.5;
  return vec2(x, y);
}

vec2 unpack_transmittance_lut_params(vec2 packed) {
  const float altitude = mix(min_altitude, max_altitude, packed.x);
  const float cos_zenith = packed.y * 2.0 - 1.0;
  return vec2(altitude, cos_zenith);
}

vec2 pack_sky_view_lut_params(float longitude, float latitude) {
  const float x = longitude / pi * 0.5 + 0.5;
  const float y = sign(latitude) * sqrt(abs(latitude) / (pi / 2.0)) * 0.5 + 0.5;
  return vec2(x, y);
}

vec2 unpack_sky_view_lut_params(vec2 packed) {
  const float longitude = (packed.x - 0.5) * 2.0 * pi;
  const float y = (packed.y - 0.5) * 2.0;
  const float latitude = sign(y) * y * y * (pi / 2.0);
  return vec2(longitude, latitude);
}

#endif
