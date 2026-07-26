#ifndef FPSPARTY_DIRECT_IRRADIANCE_COMMON_GLSL
#define FPSPARTY_DIRECT_IRRADIANCE_COMMON_GLSL

#include "descriptors.glsl"
#include "numbers.glsl"
#include "scene.glsl"
#include "atmosphere/atmosphere.glsl"

const float sun_angular_diameter = 0.0093;
const float sun_angular_radius = sun_angular_diameter / 2.0;
const float cos_sun_angular_radius = cos(sun_angular_radius);
const float sun_solid_angle = 2.0 * pi * (1.0 - cos_sun_angular_radius);

// Caps how many frames' worth of confidence the sky reservoir can
// represent, so the effective history window stays bounded. Its visibility
// is cached rather than re-traced every frame (to save a ray), so this is
// also what bounds how long a moving-shadow discrepancy can linger before
// a fresh candidate has a real chance to override it -- keep it modest.
const float max_temporal_M = 8.0;

float luminance(vec3 color) {
  return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// Never let a non-finite value (from some untested edge case) escape into
// the persistent reservoir or shading output, where it could poison every
// future frame/neighbor that reads it back -- clamp to a safe fallback
// instead. Shared by both direct_irradiance.comp and
// direct_irradiance_spatial_reuse.comp.
float sanitize(float x, float fallback) {
  return (isnan(x) || isinf(x)) ? fallback : x;
}

vec2 sanitize(vec2 x, vec2 fallback) {
  return vec2(sanitize(x.x, fallback.x), sanitize(x.y, fallback.y));
}

vec3 sanitize(vec3 x, vec3 fallback) {
  return vec3(
    sanitize(x.x, fallback.x),
    sanitize(x.y, fallback.y),
    sanitize(x.z, fallback.z));
}

// f_true(dir) = incident_radiance(dir) * cos_theta(dir), i.e. the true
// (visibility-free) rendering-equation integrand for a light sample; the
// caller multiplies in a freshly-traced (or, for biased spatial reuse,
// assumed) visibility separately. Re-evaluating this from a cached dir
// costs one texture fetch or a closed-form formula -- much cheaper than
// caching the shaded value itself, which would need to span the sun's
// ~10^7 sr^-1 radiance without overflowing fp16 reservoir storage. Both
// sun and sky share one technique-tagged reservoir (see
// direct_irradiance.comp), so call sites branch on the tag to pick which
// of these to call.
vec3 eval_f_true_sun(vec3 dir, vec3 n, vec3 sun_transmittance, Scene scene) {
  const vec3 L_sun = scene.sun_irradiance * sun_transmittance / sun_solid_angle;
  return L_sun * max(dot(n, dir), 0.0);
}

vec3 eval_f_true_sky(vec3 dir, vec3 n, Scene scene, uint sky_view_lut) {
  // textureLod, not FPSPARTY_SAMPLE/texture: this is a compute shader, so
  // there's no real screen-space quad for implicit derivatives to begin
  // with. When dir comes from a reservoir (spatially incoherent between
  // neighboring invocations, unlike a G-buffer normal) and this is called
  // from inside divergent control flow (only some invocations have valid
  // history), implicit-LOD texture() derivative computation can end up
  // reading another invocation's never-initialized dir -- undefined bits
  // feeding the LOD computation. Explicit LOD 0 sidesteps that entirely.
  const vec3 L_sky =
    textureLod(
      sampled_images[sky_view_lut],
      pack_sky_view_lut_params(
        longitude(dir), scene.camera_basis[3][1], zenith(dir)),
      0.0).rgb;
  return L_sky * max(dot(n, dir), 0.0);
}

#endif
