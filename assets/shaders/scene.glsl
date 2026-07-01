#ifndef FPSPARTY_SCENE_GLSL
#define FPSPARTY_SCENE_GLSL

#include "descriptors.glsl"
#include "extensions.glsl"

layout(std430, buffer_reference, buffer_reference_align = 16)
restrict readonly buffer Scene {
  mat4 view_projection_matrix;
  vec3 sun_irradiance;
  vec3 sun_direction;
  uint transmittance_texture;
  float animation_time;
};

#endif
