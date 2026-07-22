#version 450

#include "motion_vector.glsl"
#include "octahedral.glsl"

layout(location = 0) in vec3 in_world_normal;
layout(location = 1) in vec3 in_albedo;
layout(location = 2) in vec4 in_current_clip;
layout(location = 3) in vec4 in_previous_clip;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec2 out_normal;
layout(location = 2) out vec3 out_motion_vector;

void main() {
  out_albedo = vec4(in_albedo, 0.0);
  out_normal = oct_encode(normalize(in_world_normal));
  out_motion_vector = motion_vector(in_current_clip, in_previous_clip);
}
