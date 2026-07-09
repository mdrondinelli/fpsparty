#ifndef FPSPARTY_SRGB_GLSL
#define FPSPARTY_SRGB_GLSL

vec3 srgb_to_linear(vec3 srgb) {
  const vec3 low = srgb / 12.92f;
  const vec3 high = pow((srgb + vec3(0.055f)) / 1.055f, vec3(2.4f));
  return mix(high, low, lessThanEqual(srgb, vec3(0.04045f)));
}

vec3 linear_to_srgb(vec3 linear) {
  const vec3 low = linear * 12.92f;
  const vec3 high = 1.055f * pow(linear, vec3(1.0f / 2.4f)) - 0.055f;
  return mix(high, low, lessThanEqual(linear, vec3(0.0031308f)));
}

vec3 color_code(uint hex) {
  const vec3 srgb =
    vec3(
      (hex >> 16) & 0xFF,
      (hex >> 8) & 0xFF,
      hex & 0xFF) / 255.0f;
  return srgb_to_linear(srgb);
}

#endif
