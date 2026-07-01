#version 450

#include "atmosphere/atmosphere.glsl"
#include "mesh.glsl"
#include "numbers.glsl"

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_world_normal;
layout(location = 2) in vec3 in_albedo;

layout(location = 0) out vec4 out_color;

vec3 transmittance_along_ray(vec3 ro, vec3 rd) {
  const float h = altitude(ro);
  const float cos_zenith = dot(normalize(ro), rd);
  const vec2 lut_texcoord = pack_transmittance_lut_params(h, cos_zenith);
  return FPSPARTY_SAMPLE(
      push_constants.transmittance_texture,
      LINEAR_CLAMP,
      lut_texcoord).rgb;
}

void main() {
  const vec3 E_top = push_constants.scene.sun_irradiance;
  const vec3 E =
    E_top *
    transmittance_along_ray(
      vec3(0.0, r_ground + in_world_position.y, 0.0),
      push_constants.scene.sun_direction);
  const float n_dot_l =
    max(dot(in_world_normal, push_constants.scene.sun_direction), 0.0);
  const vec3 L = in_albedo / pi * E * n_dot_l;
  out_color = vec4(L, 1.0);
}
