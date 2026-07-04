#version 450

#include "../descriptors.glsl"
#include "../numbers.glsl"
#include "atmosphere.glsl"

layout(push_constant) uniform Push_constants {
  layout(offset = 0) mat4 camera_basis;
  layout(offset = 64) uint sky_view_lut;
  layout(offset = 80) vec3 sun_direction;
  layout(offset = 96) vec2 zoom;
} push_constants;

layout(location = 0) in vec2 in_ndc;

layout(location = 0) out vec4 out_color;

float sky_view_longitude(vec3 rd) {
  const vec3 horizontal = vec3(rd.x, 0.0, rd.z);
  const float len = length(horizontal);
  if (len < 1.0e-4) {
    return 0.0;
  }
  const vec3 normalized_horizontal = horizontal / len;
  return atan(normalized_horizontal.z, normalized_horizontal.x);
}

void main() {
  const vec3 camera_position = push_constants.camera_basis[3].xyz;
  const vec3 rd = normalize(
    mat3(push_constants.camera_basis) *
    vec3(vec2(-in_ndc.x, -in_ndc.y) * push_constants.zoom, 1.0));
  const float longitude = sky_view_longitude(rd);
  const float zenith = acos(clamp(rd.y, -1.0, 1.0));
  const vec2 lut_texcoord =
    pack_sky_view_lut_params(longitude, camera_position.y, zenith);
  out_color = FPSPARTY_SAMPLE(push_constants.sky_view_lut, lut_texcoord);
}
