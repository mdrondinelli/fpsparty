#ifndef FPSPARTY_DESCRIPTORS_GLSL
#define FPSPARTY_DESCRIPTORS_GLSL

#include "extensions.glsl"

layout(set = 0, binding = 0)
uniform sampler2D combined_images[];

#define FPSPARTY_SAMPLE(index, texcoord)                                       \
  texture(combined_images[index], texcoord)

layout(set = 0, binding = 1)
restrict uniform image2D storage_images[];

#endif
