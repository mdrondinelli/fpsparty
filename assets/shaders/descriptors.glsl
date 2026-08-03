#ifndef FPSPARTY_DESCRIPTORS_GLSL
#define FPSPARTY_DESCRIPTORS_GLSL

#include "extensions.glsl"

const int sampled_image_count = 1024;

layout(set = 0, binding = 0)
uniform sampler2D sampled_images[sampled_image_count];

// For a genuinely non-uniform index (varying per-invocation, e.g. a
// per-fragment material index -- see grid.frag), index with
// nonuniformEXT(index), not a bare index: nonuniformEXT decorates the
// index expression at the point of the array access, and that effect
// doesn't propagate through a function call boundary (confirmed via
// spirv-dis: routing it through a wrapper function drops both the
// SampledImageArrayNonUniformIndexing capability and the NonUniform
// decoration chain into the sample instruction) -- so this has to stay a
// raw texture(sampled_images[...], ...) call at every call site, not a
// shared helper.

const int storage_image_count = 1024;

layout(set = 0, binding = 1)
restrict uniform image2D storage_images[storage_image_count];

layout(set = 0, binding = 1)
restrict uniform uimage2D storage_uimages[storage_image_count];

#endif
