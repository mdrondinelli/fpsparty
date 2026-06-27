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

float altitude(vec3 position) {
  return length(position) - r_ground;
}

float rayleigh_phase(float cos_theta) {
  return 3.0f / (16.0f * pi) * (1.0f + cos_theta * cos_theta);
}

float mie_phase(float cos_theta) {
  const float g = 0.8f;
  return 3.0f / (8.0f * pi) * (1.0f - g * g) * (1.0f + cos_theta * cos_theta) /
    ((2.0f + g * g) * pow(1.0f + g * g - 2.0f * g * cos_theta, 1.5f));
}

float rayleigh_density(float altitude) {
  return exp(-altitude / 8000.0f);
}

float mie_density(float altitude) {
  return exp(-altitude / 1200.0f);
}

float ozone_density(float altitude) {
  return max(0.0f, 1.0f - abs(altitude - 25000.0f) / 15000.0f);
}

#endif
