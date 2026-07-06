#ifndef FPSPARTY_OCTAHEDRAL_GLSL
#define FPSPARTY_OCTAHEDRAL_GLSL

vec2 oct_encode(vec3 n) {
  n /= abs(n.x) + abs(n.y) + abs(n.z);
  n.xy = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * (step(0.0, n.xy) * 2.0 - 1.0); 
  return n.xy;
}

vec3 oct_decode(vec2 e) {
  vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  const float t = max(-n.z, 0.0);
  n.xy += (step(0.0, n.xy) * 2.0 - 1.0) * vec2(t);
  return normalize(n);
}

#endif
