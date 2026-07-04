#ifndef FPSPARTY_GRAPHICS_SAMPLER_HPP
#define FPSPARTY_GRAPHICS_SAMPLER_HPP

namespace fpsparty::graphics {

// Fixed samplers baked into combined-image-sampler descriptors at creation
// time. Order must match make_samplers() in descriptor_heap.cpp.
enum class Sampler {
  nearest,
  nearest_clamp,
  linear,
  linear_clamp,
  lat_long,
};

} // namespace fpsparty::graphics

#endif
