#ifndef FPSPARTY_DESCRIPTORS_GLSL
#define FPSPARTY_DESCRIPTORS_GLSL

#include "extensions.glsl"

layout(descriptor_heap) uniform sampler samplers[];

#define FPSPARTY_SAMPLER_NEAREST samplers[0]
#define FPSPARTY_SAMPLER_NEAREST_CLAMP samplers[1]
#define FPSPARTY_SAMPLER_LINEAR samplers[2]
#define FPSPARTY_SAMPLER_LINEAR_CLAMP samplers[3]

layout(descriptor_heap) uniform texture2D sampled_images[];

#define FPSPARTY_SAMPLE(index, sampler, texcoord) \
  texture(sampler2D(sampled_images[index], FPSPARTY_SAMPLER_##sampler), texcoord)

#define DEFINE_STORAGE_IMAGE_ARRAY(format) \
  layout(format, descriptor_heap) restrict uniform image2D format##_storage_images[];

DEFINE_STORAGE_IMAGE_ARRAY(rgba8)
DEFINE_STORAGE_IMAGE_ARRAY(rgba16f)
DEFINE_STORAGE_IMAGE_ARRAY(rgba32f)

#undef DEFINE_STORAGE_IMAGE_ARRAY

#endif
