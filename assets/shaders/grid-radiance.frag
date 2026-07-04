#version 450

#include "numbers.glsl"
#include "grid.glsl"

#include "atmosphere/atmosphere.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) flat in uint in_texture;

layout(location = 0) out vec4 out_color;

vec3 transmittance_along_ray(vec3 ro, vec3 rd) {
  const float h = altitude(ro);
  const float cos_zenith = dot(normalize(ro), rd);
  const vec2 lut_texcoord = pack_transmittance_lut_params(h, cos_zenith);
  return FPSPARTY_SAMPLE(
      push_constants.scene.transmittance_texture,
      lut_texcoord).rgb;
}

vec3 sky_irradiance(vec3 n) {
  return sample_sky_irradiance(push_constants.scene, n);
}

float sun_shadow(vec3 world_position) {
  const float view_depth = dot(
    world_position - push_constants.scene.camera_position,
    push_constants.scene.camera_forward);
  const uint cascade_index =
    view_depth <= push_constants.scene.shadow_cascade_split_depths.x
      ? 0u
      : view_depth <= push_constants.scene.shadow_cascade_split_depths.y ? 1u
                                                                         : 2u;
  const vec4 shadow_clip =
    push_constants.scene.shadow_view_projection_matrices[cascade_index] *
    vec4(world_position + push_constants.scene.sun_direction / 32.0, 1.0);
  const vec3 shadow_ndc = shadow_clip.xyz / shadow_clip.w;
  const vec2 shadow_texcoord = shadow_ndc.xy * 0.5 + 0.5;
  if (
    any(lessThan(shadow_texcoord, vec2(0.0))) ||
    any(greaterThan(shadow_texcoord, vec2(1.0))) ||
    shadow_ndc.z < 0.0 ||
    shadow_ndc.z > 1.0) {
    return 1.0;
  }
  const float shadow_depth =
    FPSPARTY_SAMPLE(
      nonuniformEXT(push_constants.scene.shadow_map_textures[cascade_index]),
      shadow_texcoord).r;
  return shadow_ndc.z >= shadow_depth ? 1.0 : 0.0;
}

void main() {
  const vec3 base_color =
    FPSPARTY_SAMPLE(nonuniformEXT(in_texture), in_texcoord).rgb;
  const vec3 n = vec3(
    push_constants.normal_x,
    push_constants.normal_y,
    push_constants.normal_z);
  const vec3 l = push_constants.scene.sun_direction;
  const float n_dot_l = max(dot(n, l), 0.0);
  const vec3 E_top = push_constants.scene.sun_irradiance;
  const vec3 E =
    E_top *
    transmittance_along_ray(vec3(0.0, r_ground + in_position.y, 0.0f), l) *
    n_dot_l *
    sun_shadow(in_position + n / 16.0);
  const vec3 L = base_color / pi * (E + sky_irradiance(n));
  out_color = vec4(L, 1.0f);
}
