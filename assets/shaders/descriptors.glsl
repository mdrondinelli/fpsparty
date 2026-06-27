#ifndef FPSPARTY_DESCRIPTORS_GLSL
#define FPSPARTY_DESCRIPTORS_GLSL

#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require

layout(descriptor_heap) uniform sampler samplers[];

#define FPSPARTY_SAMPLER_NEAREST samplers[0]
#define FPSPARTY_SAMPLER_LINEAR samplers[1]

layout(descriptor_heap) uniform texture2D sampled_images[];

#define DEFINE_STORAGE_IMAGE_ARRAY(format) \
  layout(format, descriptor_heap) restrict uniform image2D format##_storage_images[];

DEFINE_STORAGE_IMAGE_ARRAY(rgba8)
DEFINE_STORAGE_IMAGE_ARRAY(rgba16f)

#undef DEFINE_STORAGE_IMAGE_ARRAY

#endif
