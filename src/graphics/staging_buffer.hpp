#ifndef FPSPARTY_GRAPHICS_STAGING_BUFFER_HPP
#define FPSPARTY_GRAPHICS_STAGING_BUFFER_HPP

#include "graphics/graphics_buffer.hpp"
#include "rc.hpp"
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Staging_buffer : public rc::Object<Staging_buffer>,
                       public Graphics_buffer {
public:
  explicit Staging_buffer(std::span<const std::byte> data);
};
} // namespace fpsparty::graphics

#endif
