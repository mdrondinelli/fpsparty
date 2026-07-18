#ifndef FPSPARTY_INTERSECTORS_GLSL
#define FPSPARTY_INTERSECTORS_GLSL

vec3 offset_ray_origin(vec3 p, vec3 n) {
  return p + n / 1024.0;
    /*
  const float origin = 1.0 / 32.0;
  const float float_scale = 1.0 / 65536.0;
  const float int_scale = 256.0;
  const ivec3 of_i = ivec3(int_scale * n);
  const vec3 p_i = 
    intBitsToFloat(floatBitsToInt(p) + (ivec3(step(0.0, p)) * 2 - 1) * of_i);
  return vec3(
      abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
      abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
      abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
      */
}

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

float ray_aabb(
    vec3 ro,
    vec3 inv_rd,
    vec3 aabb_center,
    vec3 aabb_half_extents,
    out vec3 normal) {
  const vec3 n = inv_rd * (ro - aabb_center);
  const vec3 k = abs(inv_rd) * aabb_half_extents;
  const vec3 t1 = -n - k;
  const vec3 t2 = -n + k;
  const float tN = max(max(t1.x, t1.y), t1.z);
  const float tF = min(min(t2.x, t2.y), t2.z);
  if (tN > tF || tF < 0.0) {
    normal = vec3(0.0);
    return 1.0 / 0.0;
  }
  normal = -sign(inv_rd) * step(t1.yzx, t1.xyz) * step(t1.zxy, t1.xyz);
  return tN >= 0.0 ? tN : tF;
}

#endif
