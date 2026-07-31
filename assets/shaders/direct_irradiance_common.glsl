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

// One un-traced candidate for one technique: direct_irradiance.comp (pass
// 1) picks a technique per pixel and appends the sample into that
// technique's own buffer (sun_samples or sky_samples) via atomicAdd on
// count -- which buffer it lands in is the technique, so no in-struct flag
// is needed. direct_irradiance_trace_sun.comp / direct_irradiance_trace_sky
// .comp (pass 2, one dispatch per technique) read their buffer back in flat
// index order (not screen-space tile order), trace, shade, and write the
// result to direct_irradiance_image at the stored pixel. pixel is packed as
// y*width+x rather than an ivec2, so this struct's size doesn't depend on
// knowing the framebuffer width up front -- pass 2 unpacks it using
// direct_irradiance_image's own width.
// uv holds the two uniform random numbers pass 1 drew to build dir
// (square_to_cone for sun, square_to_cosine_hemisphere for sky), packed via
// packUnorm2x16, rather than the resulting unit vector itself -- each trace
// shader redoes its own (technique-fixed) warp from uv against its own
// local basis (make_basis(sun_direction) or make_basis(n)) instead of
// storing dir directly. This is both smaller (packUnorm2x16(uv) is one
// uint vs. vec3 dir's three floats) and more accurate per stored bit: 16
// bits per uv component quantizes the *input* to a well-conditioned warp,
// where oct-encoding would instead quantize dir itself and add the oct
// map's own distortion on top.
struct Distant_light_sample {
  uint pixel;
  uint uv;
};

layout(scalar, buffer_reference, buffer_reference_align = 4)
restrict buffer Distant_light_samples {
  uint count;
  Distant_light_sample samples[];
};

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
// caller multiplies in a freshly-traced visibility separately.
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
