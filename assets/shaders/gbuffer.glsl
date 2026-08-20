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

// Depth comes from the hardware depth attachment (reverse-Z, written for
// free by the depth test -- no explicit shader output needed), sampled
// directly by readers rather than duplicated into a color-attachment
// channel. Normal (r16g16_sfloat, oct-encoded) and gradient (r32_sfloat)
// are separate targets, each written explicitly.

vec2 gbuffer_encode_normal(vec3 normal) {
  return oct_encode(normal);
}

// gradient: normalized isotropic relative depth derivative (max of
// |d(linear_depth)|/linear_depth over screen x/y) -- stored normalized
// since it's scale-invariant and precision-friendly across depth range.
// Floored at gbuffer_gradient_epsilon: a true-zero gradient (flat surface,
// or underflow) collapses depth_weight's denominator down to just
// depth_weight_epsilon, making it reject neighboring taps far too
// aggressively on any tiny depth difference.
float gbuffer_encode_gradient(float gradient) {
  return max(gradient, gbuffer_gradient_epsilon);
}

// raw_depth: the hardware depth attachment's raw reverse-Z value (z_near /
// linear_depth for this projection). z_near reconstructs linear_depth.
// raw_depth == 0.0 is the "no geometry" sentinel (matches this
// projection's reverse-Z convention -- and the depth attachment's clear
// value, see Work_recorder::begin_rendering).
Gbuffer_sample gbuffer_decode(
    float raw_depth, float z_near, vec2 normal, float gradient_normalized) {
  const float linear_depth = raw_depth > 0.0 ? z_near / raw_depth : 0.0;
  return Gbuffer_sample(
    oct_decode(normal), linear_depth, gradient_normalized * linear_depth);
}

#endif
