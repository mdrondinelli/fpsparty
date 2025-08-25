#ifndef FPSPARTY_GRAPHICS_IMAGE_FORMAT_HPP
#define FPSPARTY_GRAPHICS_IMAGE_FORMAT_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Image_format {
  b8g8r8a8_srgb = static_cast<int>(vk::Format::eB8G8R8A8Srgb),
};
}

#endif
