#version 450

#include "octahedral.glsl"

layout(location = 0) in vec3 in_world_normal;
layout(location = 1) in vec3 in_albedo;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec2 out_normal;

void main() {
  out_albedo = vec4(in_albedo, 1.0);
  out_normal = oct_encode(normalize(in_world_normal));
}
