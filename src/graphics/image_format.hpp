#ifndef FPSPARTY_GRAPHICS_IMAGE_FORMAT_HPP
#define FPSPARTY_GRAPHICS_IMAGE_FORMAT_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Image_format {
  b8g8r8a8_srgb = static_cast<int>(vk::Format::eB8G8R8A8Srgb),
  d32_sfloat = static_cast<int>(vk::Format::eD32Sfloat),
};

namespace detail {
constexpr vk::ImageAspectFlags
get_image_format_vk_image_aspect_flags(Image_format format) {
  switch (format) {
  case Image_format::b8g8r8a8_srgb:
    return vk::ImageAspectFlagBits::eColor;
  case Image_format::d32_sfloat:
    return vk::ImageAspectFlagBits::eDepth;
  default:
    return {};
  }
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
