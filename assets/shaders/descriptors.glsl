#ifndef FPSPARTY_DESCRIPTORS_GLSL
#define FPSPARTY_DESCRIPTORS_GLSL

#include "extensions.glsl"

// Combined image samplers. The sampler is baked into each descriptor when it is
// created on the host side (see graphics::Sampler).
layout(set = 0, binding = 0) uniform sampler2D combined_images[];

#define FPSPARTY_SAMPLE(index, texcoord)                                       \
  texture(combined_images[index], texcoord)

#define DEFINE_STORAGE_IMAGE_ARRAY(format, binding_index)                      \
  layout(set = 0, binding = binding_index, format) restrict uniform image2D    \
    format##_storage_images[];

DEFINE_STORAGE_IMAGE_ARRAY(rgba8, 1)
DEFINE_STORAGE_IMAGE_ARRAY(rgba16f, 2)
DEFINE_STORAGE_IMAGE_ARRAY(rgba32f, 3)

#undef DEFINE_STORAGE_IMAGE_ARRAY

#endif
