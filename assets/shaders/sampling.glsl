#ifndef FPSPARTY_SAMPLING_GLSL
#define FPSPARTY_SAMPLING_GLSL

#include "numbers.glsl"

mat3 make_basis(vec3 n) {
  const vec3 helper = abs(n.x) < abs(n.y) ? vec3(1, 0, 0) : vec3(0, 1, 0);
  const vec3 t = normalize(cross(helper, n));
  const vec3 b = cross(n, t);
  return mat3(t, b, n);
}

vec2 square_to_disk(vec2 u) {
  const float a = 2 * u.x - 1;
  const float b = 2 * u.y - 1;
  float r;
  float phi;
  if (a * a > b * b) {
    r = a;
    phi = pi / 4 * (b / a);
  } else {
    r = b;
    phi = pi / 2 - pi / 4 * (a / b);
  }
  return r * vec2(cos(phi), sin(phi));
}

// Samples a cosine-weighted direction in the hemisphere around the z axis;
// pdf = cos_theta / pi
vec3 square_to_cosine_hemisphere(vec2 u) {
  const vec2 d = square_to_disk(u);
  return vec3(d, sqrt(max(0.0, 1.0 - dot(d, d))));
}

// Samples a direction in a cone around the z axis with spread angle theta_max
vec3 square_to_cone(vec2 u, float cos_theta_max) {
  const float cos_theta = (1 - u.x) + u.x * cos_theta_max;
  const float sin_theta = sqrt(1 - cos_theta * cos_theta);
  const float phi = u.y * 2 * pi;
  return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

#endif
