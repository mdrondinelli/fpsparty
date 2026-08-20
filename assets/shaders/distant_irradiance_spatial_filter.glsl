#ifndef FPSPARTY_DISTANT_IRRADIANCE_SPATIAL_FILTER_GLSL
#define FPSPARTY_DISTANT_IRRADIANCE_SPATIAL_FILTER_GLSL

const float depth_weight_sigma_z = 1.0;
const float depth_weight_epsilon = 1e-4;

// Depth edge-stopping weight between sample positions p and q, given their
// depths and the isotropic depth-gradient magnitude at p -- expected
// depth change over |p - q| is gradient_z_p * length(p - q), not a
// directional dot product (no direction to dot against once the gradient
// is a magnitude, not a vector).
float depth_weight(
    vec2 p, vec2 q, float z_p, float z_q, float gradient_z_p) {
  return exp(
    -abs(z_p - z_q) /
    (depth_weight_sigma_z * gradient_z_p * length(p - q) +
     depth_weight_epsilon));
}

const float normal_weight_sigma_n = 128.0;

// Normal edge-stopping weight between sample normals n_p and n_q.
float normal_weight(vec3 n_p, vec3 n_q) {
  return pow(max(0.0, dot(n_p, n_q)), normal_weight_sigma_n);
}

const float luminance_weight_sigma_l = 4.0;
const float luminance_weight_epsilon = 1e-4;

// Luminance edge-stopping weight between sample luminances l_p and l_q,
// using the blurred variance at p (distant_irradiance_variance.comp's output)
// to scale the tolerance.
float luminance_weight(float l_p, float l_q, float blurred_variance_p) {
  return exp(
    -abs(l_p - l_q) /
    (luminance_weight_sigma_l * sqrt(blurred_variance_p) +
     luminance_weight_epsilon));
}

#endif
