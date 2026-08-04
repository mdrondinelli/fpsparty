#version 450

#include "grid.glsl"
#include "motion_vector.glsl"
#include "octahedral.glsl"

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) flat in uint in_texture;
layout(location = 2) in vec4 in_current_clip;
layout(location = 3) in vec4 in_previous_clip;
layout(location = 4) flat in float in_emissivity_scale;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec2 out_normal;
layout(location = 2) out vec3 out_motion_vector;
layout(location = 3) out vec2 out_depth_gradient;

void main() {
  // Inline, not a helper: nonuniformEXT is a source-level qualifier on the
  // index expression at the point of the array access -- routing it
  // through a function call severs that (confirmed via spirv-dis: the
  // SampledImageArrayNonUniformIndexing capability and the NonUniform
  // decoration chain into the actual OpImageSampleImplicitLod both
  // disappear). in_texture is a per-fragment varying, genuinely
  // non-uniform, so this one has to stay inline.
  const vec3 base_color =
    texture(
      sampler2D(sampled_images[nonuniformEXT(in_texture)], SAMPLER_NEAREST),
      in_texcoord).rgb;
  const vec3 n = vec3(
    push_constants.normal_x,
    push_constants.normal_y,
    push_constants.normal_z);
  out_albedo = vec4(base_color, in_emissivity_scale);
  out_normal = oct_encode(n);
  out_motion_vector = motion_vector(in_current_clip, in_previous_clip);
  out_depth_gradient = vec2(dFdx(gl_FragCoord.z), dFdy(gl_FragCoord.z));
}
