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
  // Ambient cube: 6 cardinal directions x RGB, written by
  // atmosphere/sky_irradiance.comp. Shaders read this through a separate
  // Sky_irradiance buffer_reference (see atmosphere/sky_irradiance.glsl)
  // passed to them directly, host-offset into this same buffer via
  // scene_sky_irradiance_offset in application.cpp -- not accessed through
  // this Scene struct directly (keeps atmosphere/*.glsl from needing to
  // know about Scene at all).
  float sky_irradiance[18];
};

#endif
