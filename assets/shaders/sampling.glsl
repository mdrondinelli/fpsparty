#ifndef FPSPARTY_SAMPLING_GLSL
#define FPSPARTY_SAMPLING_GLSL

#include "numbers.glsl"

mat3 make_basis(vec3 n) {
  const vec3 t = abs(n.x) < abs(n.y) ? vec3(1, 0, 0) : vec3(0, 1, 0);
  const vec3 b = cross(n, t);
  return mat3(t, b, n);
}

// Samples a direction in a cone around the z axis with spread angle theta_max
vec3 square_to_cone(vec2 u, float cos_theta_max) {
  const float cos_theta = (1 - u.x) + u.x * cos_theta_max;
  const float sin_theta = sqrt(1 - cos_theta * cos_theta);
  const float phi = u.y * 2 * pi;
  return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

#endif
