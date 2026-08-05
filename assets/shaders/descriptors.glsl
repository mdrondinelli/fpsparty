#ifndef FPSPARTY_DESCRIPTORS_GLSL
#define FPSPARTY_DESCRIPTORS_GLSL

#include "extensions.glsl"

const int sampled_image_count = 1024;

// Sampler-less: combine with a sampler at each call site via
// sampler2D(sampled_images[i], SAMPLER_X).
layout(set = 0, binding = 0)
uniform texture2D sampled_images[sampled_image_count];

// Order must match graphics::Sampler / make_samplers() in
// descriptor_heap.cpp. Each macro expands to the full indexing
// expression -- call sites read SAMPLER_LAT_LONG, not
// samplers[SAMPLER_LAT_LONG].
const int sampler_count = 5;

layout(set = 0, binding = 2)
uniform sampler samplers[sampler_count];

#define SAMPLER_NEAREST samplers[0]
#define SAMPLER_NEAREST_CLAMP samplers[1]
#define SAMPLER_LINEAR samplers[2]
#define SAMPLER_LINEAR_CLAMP samplers[3]
#define SAMPLER_LAT_LONG samplers[4]

// For a genuinely non-uniform index (e.g. a per-fragment material index
// -- see grid.frag), index with nonuniformEXT(index). The decoration
// doesn't survive a function-call boundary (verified via spirv-dis), so
// keep such accesses inline, never behind a shared helper.

const int storage_image_count = 1024;

layout(set = 0, binding = 1)
restrict uniform image2D storage_images[storage_image_count];

layout(set = 0, binding = 1)
restrict uniform uimage2D storage_uimages[storage_image_count];

#endif
