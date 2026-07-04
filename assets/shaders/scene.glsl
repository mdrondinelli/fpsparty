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
  float sky_irradiance[18];
  mat4 shadow_view_projection_matrices[3];
  uint shadow_map_textures[3];
  vec2 shadow_cascade_split_depths;
  vec3 camera_position;
  vec3 camera_forward;
};

vec3 sample_sky_irradiance(Scene scene, vec3 direction) {
  const vec3 pos_x = vec3(
    scene.sky_irradiance[0],
    scene.sky_irradiance[1],
    scene.sky_irradiance[2]);
  const vec3 neg_x = vec3(
    scene.sky_irradiance[3],
    scene.sky_irradiance[4],
    scene.sky_irradiance[5]);
  const vec3 pos_y = vec3(
    scene.sky_irradiance[6],
    scene.sky_irradiance[7],
    scene.sky_irradiance[8]);
  const vec3 neg_y = vec3(
    scene.sky_irradiance[9],
    scene.sky_irradiance[10],
    scene.sky_irradiance[11]);
  const vec3 pos_z = vec3(
    scene.sky_irradiance[12],
    scene.sky_irradiance[13],
    scene.sky_irradiance[14]);
  const vec3 neg_z = vec3(
    scene.sky_irradiance[15],
    scene.sky_irradiance[16],
    scene.sky_irradiance[17]);
  const vec3 x = mix(neg_x, pos_x, bvec3(direction.x > 0.0));
  const vec3 y = mix(neg_y, pos_y, bvec3(direction.y > 0.0));
  const vec3 z = mix(neg_z, pos_z, bvec3(direction.z > 0.0));
  return mat3(x, y, z) * (direction * direction);
}

#endif
