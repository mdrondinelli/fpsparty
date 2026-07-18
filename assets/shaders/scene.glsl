#ifndef FPSPARTY_SCENE_GLSL
#define FPSPARTY_SCENE_GLSL

#include "extensions.glsl"

layout(std430, buffer_reference, buffer_reference_align = 16)
restrict readonly buffer Scene {
  mat4 view_projection_matrix;
  mat4 previous_view_projection_matrix;
  float animation_time;
  layout(row_major) mat4x3 camera_basis;
  vec3 sun_direction;
  vec3 sun_irradiance;
  vec2 zoom;
  float z_near;
};

#endif
