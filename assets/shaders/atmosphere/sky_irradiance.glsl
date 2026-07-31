#ifndef FPSPARTY_ATMOSPHERE_SKY_IRRADIANCE_GLSL
#define FPSPARTY_ATMOSPHERE_SKY_IRRADIANCE_GLSL

#include "../extensions.glsl"

// 6 cardinal-direction (+-X, +-Y, +-Z) cosine-weighted hemisphere-
// integrated sky irradiance values, RGB each -- see sky_irradiance.comp
// for how this gets filled in.
layout(std430, buffer_reference, buffer_reference_align = 4)
restrict buffer Sky_irradiance {
  float irradiance[18];
};

// Ambient cube reconstruction for an arbitrary normal: squared-component
// blend of the 6 precomputed cardinal-direction values. Exact for
// axis-aligned block faces (n is a unit basis vector, so exactly one
// squared component is 1 and the rest are 0); a smooth, cheap
// approximation for arbitrary entity normals.
vec3 sample_sky_irradiance(Sky_irradiance sky_irradiance, vec3 n) {
  const vec3 e_pos_x = vec3(
    sky_irradiance.irradiance[0],
    sky_irradiance.irradiance[1],
    sky_irradiance.irradiance[2]);
  const vec3 e_neg_x = vec3(
    sky_irradiance.irradiance[3],
    sky_irradiance.irradiance[4],
    sky_irradiance.irradiance[5]);
  const vec3 e_pos_y = vec3(
    sky_irradiance.irradiance[6],
    sky_irradiance.irradiance[7],
    sky_irradiance.irradiance[8]);
  const vec3 e_neg_y = vec3(
    sky_irradiance.irradiance[9],
    sky_irradiance.irradiance[10],
    sky_irradiance.irradiance[11]);
  const vec3 e_pos_z = vec3(
    sky_irradiance.irradiance[12],
    sky_irradiance.irradiance[13],
    sky_irradiance.irradiance[14]);
  const vec3 e_neg_z = vec3(
    sky_irradiance.irradiance[15],
    sky_irradiance.irradiance[16],
    sky_irradiance.irradiance[17]);
  const vec3 n2 = n * n;
  return n2.x * (n.x > 0.0 ? e_pos_x : e_neg_x) +
    n2.y * (n.y > 0.0 ? e_pos_y : e_neg_y) +
    n2.z * (n.z > 0.0 ? e_pos_z : e_neg_z);
}

#endif
