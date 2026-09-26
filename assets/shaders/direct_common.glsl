#ifndef FPSPARTY_DIRECT_COMMON_GLSL
#define FPSPARTY_DIRECT_COMMON_GLSL

#include "color.glsl"
#include "descriptors.glsl"
#include "gbuffer.glsl"
#include "intersectors.glsl"
#include "numbers.glsl"
#include "random.glsl"
#include "sampling.glsl"
#include "scene.glsl"
#include "atmosphere/atmosphere.glsl"
#include "atmosphere/sky_irradiance.glsl"

const float sun_angular_diameter = 0.0093;
const float sun_angular_radius = sun_angular_diameter / 2.0;
const float cos_sun_angular_radius = cos(sun_angular_radius);
const float sun_solid_angle = 2.0 * pi * (1.0 - cos_sun_angular_radius);

// A sample is an (emitter, direction) pair, and direct irradiance is one
// integral over that space. Two techniques produce pairs, one sample each:
//
//   environment  names the distant environment, then draws w_i from its
//                own two-lobe mixture
//                  p_env = p_cone * I_disc / sun_solid_angle
//                        + (1 - p_cone) * cos_theta / pi
//   brdf         draws w_i from cos_theta / pi and traces; the first
//                emitter hit supplies the index
//
// Balance MIS over the two gives each sample the summed density at the
// pair it landed on: p_env + p_brdf. No sum over emitters appears because
// neither technique forgets which emitter it named.
//
// The environment is one emitter, not two. Sun and sky are not separable
// by direction (the sky's radiance inside the solar disc is in-scatter,
// which the sun does not occlude), so they are lobes of its sampler
// rather than emitters of their own, and every ray evaluates sky
// everywhere plus sunlight inside the disc.
//
// The traces write unweighted integrands; direct_combine.comp rebuilds
// both directions and both densities per pixel and does the division. So
// everything below that a trace and the combination both depend on lives
// here and is called from both -- if the two sides ever disagree about a
// direction or a density, the MIS weights stop summing to one and energy
// is silently lost.

// Coherence bins. The payload lives in a screen-space image, so a queue
// entry is only the pixel it belongs to.
layout(scalar, buffer_reference, buffer_reference_align = 4)
restrict buffer Direct_queue {
  uint count;
  uint pixels[];
};

ivec2 direct_queue_pixel(uint pixel_index, ivec2 size) {
  return ivec2(
    int(pixel_index % uint(size.x)), int(pixel_index / uint(size.x)));
}

// One sample's random variables, r32_uint. The two samples live in
// separate images because their consumers are separate: a trace reads one
// of them and would otherwise pull the other into cache with it, halving
// the useful texels per sector.
void direct_store_uv(uint uv_image, ivec2 pixel, vec2 uv) {
  imageStore(
    storage_uimages[uv_image], pixel, uvec4(packUnorm2x16(uv), 0u, 0u, 0u));
}

vec2 direct_load_uv(uint uv_image, ivec2 pixel) {
  return unpackUnorm2x16(imageLoad(storage_uimages[uv_image], pixel).x);
}

// The persistent BRDF trace pulls pixels from here instead of getting one
// per invocation, so a warp whose lanes finish early refills them rather
// than idling. One counter for the whole frame.
layout(scalar, buffer_reference, buffer_reference_align = 4)
restrict buffer Direct_ray_cursor {
  uint next;
};

// Steps a lane advances between refills. Larger amortizes the subgroup
// bookkeeping and rt_traverse's per-call setup; smaller returns a
// finished lane to the pool sooner.
const uint direct_traverse_step_budget = 16u;

// Refill fires once this fraction of the warp is idle. It sets how often
// the loop pays its subgroup ops against how long a finished lane waits.
// 1/1 would refill only when the whole warp is idle, which is the
// one-ray-per-invocation behaviour the persistent pool exists to avoid;
// a high fraction leans on the nothing-traversing escape below it, since
// drained lanes can otherwise hold the count short of the threshold.
// Swept at step budget 16 on the BRDF trace: 1/4 measured 1.49ms, 3/4
// measured worse still, 1/2 measured 1.35ms. Both directions cost more, so
// this is at its optimum and is not worth re-exploring without a change to
// the step budget or the pool size.
const uint direct_refill_idle_numerator = 1u;
const uint direct_refill_idle_denominator = 2u;

// Primary surface reconstructed from the G-buffer, in world space.
struct Direct_shading_point {
  vec3 position;
  vec3 normal;
  // Offset along the normal to avoid self-intersection.
  vec3 ray_origin;
};

Direct_shading_point direct_shading_point(
    Scene scene,
    uint depth_texture,
    uint normal_texture,
    ivec2 pixel,
    ivec2 size) {
  const vec2 ndc = (vec2(pixel) + 0.5) / vec2(size) * 2.0 - 1.0;
  const vec3 view_ray_origin = scene.camera_basis[3].xyz;
  const vec3 view_ray_direction =
    vec3(-ndc.x * scene.zoom.x, -ndc.y * scene.zoom.y, 1.0);
  const Gbuffer_sample g = gbuffer_decode(
    texelFetch(sampled_images[depth_texture], pixel, 0).x,
    scene.z_near,
    texelFetch(sampled_images[normal_texture], pixel, 0).xy,
    0.0);
  const vec3 position =
    view_ray_origin +
    mat3(scene.camera_basis) * (view_ray_direction * g.linear_depth);
  return Direct_shading_point(
    position, g.normal, offset_ray_origin(position, g.normal));
}

// The surface normal reaches these only as the cosine against w_i, so
// they take that directly: callers already have it, and it saves each of
// them recomputing the same dot product two or three times per ray.
vec3 eval_incident_irradiance_sun(
    float cos_theta, vec3 integrated_irradiance, vec3 transmittance) {
  const vec3 L_i = integrated_irradiance / sun_solid_angle * transmittance;
  return L_i * cos_theta;
}

vec3 eval_incident_irradiance_sky(
    vec3 w_i, float cos_theta, Scene scene, uint sky_view_lut) {
  const vec3 L_i =
    textureLod(
      sampler2D(sampled_images[sky_view_lut], SAMPLER_LAT_LONG),
      pack_sky_view_lut_params(
        longitude(w_i), scene.camera_basis[3][1], zenith(w_i)),
      0.0).rgb;
  return L_i * cos_theta;
}

// The cone lobe's mixture weight, and whether the environment is worth a
// ray at all. Recomputed rather than stored, so sample generation and the
// combination must call this with identical arguments.
struct Direct_environment_weight {
  float p_cone;
  bool valid;
};

Direct_environment_weight direct_environment_weight(
    Scene scene,
    vec3 n,
    float shading_altitude,
    uint transmittance_texture,
    Sky_irradiance sky_irradiance) {
  const vec3 sun_direction = normalize(scene.sun_direction);
  const vec3 sun_transmittance = transmittance_along_ray(
    transmittance_texture,
    vec3(0.0, r_ground + shading_altitude, 0.0),
    sun_direction);
  const float l_sun = luminance(
    eval_incident_irradiance_sun(
      max(dot(n, sun_direction), 0.0),
      scene.sun_irradiance,
      sun_transmittance) *
    sun_solid_angle);
  const float l_sky = luminance(sample_sky_irradiance(sky_irradiance, n));
  const float total = l_sun + l_sky;
  const bool valid = total > 0.0;
  return Direct_environment_weight(valid ? l_sun / total : 0.0, valid);
}

// The per-pixel draw. The combination replays it for is_cone, which the
// uv to direction mapping needs even though the mixture density does not.
struct Direct_draw {
  bool is_cone;
  vec2 environment_uv;
  vec2 brdf_uv;
};

Direct_draw direct_draw(uint pixel_index, uint seed, float p_cone) {
  Random random;
  random.state = lowbias32(pixel_index) ^ lowbias32(seed);
  const bool is_cone = random_float(random) < p_cone;
  const vec2 environment_uv =
    vec2(random_float(random), random_float(random));
  const vec2 brdf_uv = vec2(random_float(random), random_float(random));
  return Direct_draw(is_cone, environment_uv, brdf_uv);
}

vec3 direct_environment_direction(
    bool is_cone, vec3 n, vec3 sun_direction, vec2 uv) {
  return is_cone
    ? make_basis(normalize(sun_direction)) *
        square_to_cone(uv, cos_sun_angular_radius)
    : make_basis(n) * square_to_cosine_hemisphere(uv);
}

vec3 direct_brdf_direction(vec3 n, vec2 uv) {
  return make_basis(n) * square_to_cosine_hemisphere(uv);
}

bool direct_in_sun_disc(vec3 w_i, vec3 sun_direction) {
  return dot(normalize(w_i), normalize(sun_direction)) >= cos_sun_angular_radius;
}

// Radiance arriving from the environment along w_i, times the cosine.
// Visibility is the caller's: a trace multiplies by it, the combination
// never sees it.
vec3 direct_environment_integrand(
    Scene scene,
    float cos_theta,
    float shading_altitude,
    vec3 w_i,
    bool in_sun_disc,
    uint transmittance_texture,
    uint sky_view_lut) {
  vec3 integrand =
    eval_incident_irradiance_sky(w_i, cos_theta, scene, sky_view_lut);
  if (in_sun_disc) {
    const vec3 transmittance = transmittance_along_ray(
      transmittance_texture,
      vec3(0.0, r_ground + shading_altitude, 0.0),
      w_i);
    integrand += eval_incident_irradiance_sun(
      cos_theta, scene.sun_irradiance, transmittance);
  }
  return integrand;
}

float direct_environment_pdf(
    float p_cone, float cos_theta, bool in_sun_disc) {
  const float cone_density = in_sun_disc ? p_cone / sun_solid_angle : 0.0;
  return cone_density + (1.0 - p_cone) * cos_theta / pi;
}

float direct_brdf_pdf(float cos_theta) {
  return cos_theta / pi;
}

#endif
