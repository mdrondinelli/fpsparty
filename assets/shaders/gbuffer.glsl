#ifndef FPSPARTY_GBUFFER_GLSL
#define FPSPARTY_GBUFFER_GLSL

#include "octahedral.glsl"

struct Gbuffer_sample {
  vec3 normal;
  float linear_depth;
  // Absolute depth-gradient magnitude (isotropic), ready to feed
  // depth_weight -- see gbuffer_decode.
  float gradient;
};

const float gbuffer_gradient_epsilon = 1e-4;

// gradient: normalized isotropic relative depth derivative (max of
// |d(linear_depth)|/linear_depth over screen x/y) -- stored normalized
// since it's scale-invariant and precision-friendly across depth range.
// Floored at gbuffer_gradient_epsilon: a true-zero gradient (flat surface,
// or fp16 underflow) collapses depth_weight's denominator down to just
// depth_weight_epsilon, making it reject neighboring taps far too
// aggressively on any tiny depth difference.
vec4 gbuffer_encode(vec3 normal, float linear_depth, float gradient) {
  return vec4(oct_encode(normal), linear_depth, max(gradient, gbuffer_gradient_epsilon));
}

Gbuffer_sample gbuffer_decode(vec4 packed) {
  // packed.w is the normalized gradient; scale back to absolute units
  // (matching linear_depth's units) here so callers never have to.
  return Gbuffer_sample(oct_decode(packed.xy), packed.z, packed.w * packed.z);
}

#endif
