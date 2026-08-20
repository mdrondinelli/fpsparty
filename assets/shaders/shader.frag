#version 450

#include "gbuffer.glsl"
#include "mesh.glsl"
#include "motion_vector.glsl"

layout(location = 0) in vec3 in_world_normal;
layout(location = 1) in vec3 in_albedo;
layout(location = 2) in vec4 in_current_clip;
layout(location = 3) in vec4 in_previous_clip;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec2 out_normal;
layout(location = 2) out float out_gradient;
layout(location = 3) out vec3 out_motion_vector;

void main() {
  out_albedo = vec4(in_albedo, 0.0);
  // Depth itself needs no explicit output -- the hardware depth test
  // already writes it into the depth attachment for free.
  const float linear_depth = push_constants.scene.z_near / gl_FragCoord.z;
  const float gradient = max(
    abs(dFdx(linear_depth)) / linear_depth,
    abs(dFdy(linear_depth)) / linear_depth);
  out_normal = gbuffer_encode_normal(normalize(in_world_normal));
  out_gradient = gbuffer_encode_gradient(gradient);
  out_motion_vector = motion_vector(in_current_clip, in_previous_clip);
}
