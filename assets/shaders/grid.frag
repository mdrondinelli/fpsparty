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
      push_constants.transmittance_texture,
      LINEAR_CLAMP,
      lut_texcoord).rgb;
}

void main() {
  const vec3 base_color =
    FPSPARTY_SAMPLE(nonuniformEXT(in_texture), NEAREST, in_texcoord).rgb;
  const vec3 n = vec3(
    push_constants.normal_x,
    push_constants.normal_y,
    push_constants.normal_z);
  const vec3 l = push_constants.scene.sun_direction;
  const float n_dot_l = max(dot(n, l), 0.0); 
  const vec3 E_top = push_constants.scene.sun_irradiance;
  const vec3 E =
    E_top *
    transmittance_along_ray(vec3(0.0, r_ground + in_position.y, 0.0f), l);
  const vec3 L = base_color / pi * E * n_dot_l;
  out_color = vec4(L, 1.0f);
}
