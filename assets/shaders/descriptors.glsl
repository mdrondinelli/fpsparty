#ifndef FPSPARTY_DESCRIPTORS_GLSL
#define FPSPARTY_DESCRIPTORS_GLSL

#include "extensions.glsl"

const int combined_image_count = 1024;

layout(set = 0, binding = 0)
uniform sampler2D combined_images[combined_image_count];

#define FPSPARTY_SAMPLE(index, texcoord)                                       \
  texture(combined_images[index], texcoord)

const int storage_image_count = 1024;

layout(set = 0, binding = 1)
restrict uniform image2D storage_images[storage_image_count];

#endif
