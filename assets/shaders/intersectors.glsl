#ifndef FPSPARTY_INTERSECTORS_GLSL
#define FPSPARTY_INTERSECTORS_GLSL

float ray_aabb(
    vec3 ro,
    vec3 inv_rd,
    vec3 aabb_center,
    vec3 aabb_half_extents) {
  const vec3 n = inv_rd * (ro - aabb_center);
  const vec3 k = abs(inv_rd) * aabb_half_extents;
  const vec3 t1 = -n - k;
  const vec3 t2 = -n + k;
  const float tN = max(max(t1.x, t1.y), t1.z);
  const float tF = min(min(t2.x, t2.y), t2.z);
  if (tN > tF || tF < 0.0) {
    return 1.0 / 0.0;
  }
  return tN >= 0.0 ? tN : tF;
}

#endif
