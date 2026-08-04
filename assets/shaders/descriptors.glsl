#ifndef FPSPARTY_DESCRIPTORS_GLSL
#define FPSPARTY_DESCRIPTORS_GLSL

#include "extensions.glsl"

const int sampled_image_count = 1024;

// Sampler-less: the sampler is combined at each call site via
// sampler2D(sampled_images[i], SAMPLER_X) instead of being baked into
// the descriptor -- descriptors are now allocated automatically when an
// Image is created (see graphics::Image::allocate_descriptors), and
// image creation shouldn't need to know which sampler a given use site
// wants.
layout(set = 0, binding = 0)
uniform texture2D sampled_images[sampled_image_count];

// Fixed, immutable set of samplers -- order must match graphics::Sampler
// / make_samplers() in descriptor_heap.cpp. Each macro expands to the
// full indexing expression, not just the index, so call sites read
// SAMPLER_LAT_LONG directly, not samplers[SAMPLER_LAT_LONG].
const int sampler_count = 5;

layout(set = 0, binding = 2)
uniform sampler samplers[sampler_count];

#define SAMPLER_NEAREST samplers[0]
#define SAMPLER_NEAREST_CLAMP samplers[1]
#define SAMPLER_LINEAR samplers[2]
#define SAMPLER_LINEAR_CLAMP samplers[3]
#define SAMPLER_LAT_LONG samplers[4]

// For a genuinely non-uniform index (varying per-invocation, e.g. a
// per-fragment material index -- see grid.frag), index with
// nonuniformEXT(index), not a bare index: nonuniformEXT decorates the
// index expression at the point of the array access, and that effect
// doesn't propagate through a function call boundary (confirmed via
// spirv-dis: routing it through a wrapper function drops both the
// SampledImageArrayNonUniformIndexing capability and the NonUniform
// decoration chain into the sample instruction) -- so this has to stay a
// raw texture(sampler2D(sampled_images[...], SAMPLER_X), ...) call at
// every call site, not a shared helper.

const int storage_image_count = 1024;

layout(set = 0, binding = 1)
restrict uniform image2D storage_images[storage_image_count];

layout(set = 0, binding = 1)
restrict uniform uimage2D storage_uimages[storage_image_count];

#endif
