#ifndef FPSPARTY_DISTANT_IRRADIANCE_SPATIAL_FILTER_GLSL
#define FPSPARTY_DISTANT_IRRADIANCE_SPATIAL_FILTER_GLSL

// The three edge-stopping weights are all of the form exp(-x), so each is
// split into the exponent it contributes. The caller sums the exponents
// and takes a single exp, rather than one exp per weight per tap.
//
// Terms that depend only on p are lifted into *_rcp_scale helpers, called
// once before the tap loop; the per-tap functions take the reciprocal and
// multiply. This is not a shortcut the compiler takes on its own -- the
// loop-invariant sqrt in luminance_weight was being emitted inside the tap
// loop.

const float depth_weight_sigma_z = 1.0;
const float depth_weight_epsilon = 1e-4;

// Per-tap reciprocal scale for the depth exponent. dist_pq is |p - q| in
// pixels; the expected depth change over it is gradient_z_p * dist_pq, not
// a directional dot product (no direction to dot against once the gradient
// is a magnitude, not a vector). gradient_z_p is folded into
// depth_weight_sigma_z by the caller.
float depth_weight_rcp_scale(float sigma_gradient_z_p, float dist_pq) {
  return 1.0 / (sigma_gradient_z_p * dist_pq + depth_weight_epsilon);
}

// Depth edge-stopping exponent between sample depths z_p and z_q.
float depth_weight_exponent(float z_p, float z_q, float rcp_scale) {
  return abs(z_p - z_q) * rcp_scale;
}

// Normal edge-stopping weight between sample normals n_p and n_q: the
// clamped dot raised to sigma_n = 128. Not an exponential, so it stays a
// factor rather than an exponent.
//
// Written as an explicit chain of squarings -- 7 multiplies in place of
// pow()'s log/exp pair -- rather than a loop over a count, because glslc
// leaves such a loop rolled inside the tap loop. Each step doubles the
// exponent, so the chain length is the only thing setting sigma_n; there
// is deliberately no separate constant to fall out of step with it.
float normal_weight(vec3 n_p, vec3 n_q) {
  float w = max(0.0, dot(n_p, n_q));
  w *= w; // ^2
  w *= w; // ^4
  w *= w; // ^8
  w *= w; // ^16
  w *= w; // ^32
  w *= w; // ^64
  return w * w; // ^128
}

const float luminance_weight_sigma_l = 4.0;
const float luminance_weight_epsilon = 1e-4;

// Reciprocal luminance tolerance at p, scaled by the blurred variance
// there (distant_irradiance_variance.comp's output). Depends only on p, so
// the sqrt and the divide are paid once per pixel, not once per tap.
float luminance_weight_rcp_scale(float blurred_variance_p) {
  return 1.0 / (luminance_weight_sigma_l * sqrt(blurred_variance_p) +
                luminance_weight_epsilon);
}

// Luminance edge-stopping exponent between sample luminances l_p and l_q.
float luminance_weight_exponent(float l_p, float l_q, float rcp_scale) {
  return abs(l_p - l_q) * rcp_scale;
}

#endif
