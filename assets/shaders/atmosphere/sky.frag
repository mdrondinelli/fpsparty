#version 450

#include "../descriptors.glsl"
#include "atmosphere.glsl"

layout(push_constant) uniform Push_constants {
  layout(offset = 0) mat4 camera_basis;
  layout(offset = 64) vec2 zoom;
  layout(offset = 72) float altitude;
  layout(offset = 76) uint transmittance_lut;
} push_constants;

layout(location = 0) in vec2 in_ndc;

layout(location = 0) out vec4 out_color;

// ro must be inside the atmosphere
vec3 ray_atmosphere(vec3 ro, vec3 rd) {
  const float b = dot(ro, rd);
  const float c = dot(ro, ro) - r_atmosphere * r_atmosphere;
  const float h = b * b - c;
  const float t = -b + sqrt(h);
  return ro + t * rd;
}

void main() {
  const vec3 rd = mat3(push_constants.camera_basis) * normalize(
    vec3(vec2(-in_ndc.x, -in_ndc.y) * push_constants.zoom, 1.0f));
  const vec2 lut_texcoord =
    pack_transmittance_lut_params(push_constants.altitude, rd.y);
  const vec3 transmittance =
    FPSPARTY_SAMPLE(
      push_constants.transmittance_lut,
      LINEAR_CLAMP,
      lut_texcoord).rgb;
  out_color = 256.0f * vec4(transmittance, 1.0f);
}
