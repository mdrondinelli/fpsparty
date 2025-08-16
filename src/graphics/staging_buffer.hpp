#ifndef FPSPARTY_GRAPHICS_STAGING_BUFFER_HPP
#define FPSPARTY_GRAPHICS_STAGING_BUFFER_HPP

#include "graphics/buffer.hpp"
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Staging_buffer : public Buffer {
public:
  explicit Staging_buffer(std::span<const std::byte> data);
};
} // namespace fpsparty::graphics

#endif
