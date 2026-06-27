#version 450

#include "../descriptors.glsl"
#include "atmosphere.glsl"

layout(push_constant) uniform Push_constants {
  layout(offset = 0) vec2 zoom;
  layout(offset = 8) float altitude;
  layout(offset = 12) uint transmittance_lut;
} push_constants;

layout(location = 0) in vec2 in_ndc;

layout(location = 0) out vec4 out_color;

void main() {
  const vec3 rd = normalize(
    vec3(vec2(in_ndc.x, 1.0f - in_ndc.y) * push_constants.zoom, 1.0f));
  const vec2 lut_texcoord =
    pack_transmittance_lut_params(push_constants.altitude, rd.y);
  const vec3 transmittance =
    FPSPARTY_SAMPLE(
      push_constants.transmittance_lut,
      NEAREST,
      lut_texcoord).rgb;
  out_color = 1024.0f * vec4(transmittance, 1.0f);
}
