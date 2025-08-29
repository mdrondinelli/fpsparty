#ifndef FPSPARTY_GRAPHICS_IMAGE_USAGE_HPP
#define FPSPARTY_GRAPHICS_IMAGE_USAGE_HPP

#include "flags.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Image_usage_flag_bits {
  sampled = static_cast<int>(vk::ImageUsageFlagBits::eSampled),
  color_attachment = static_cast<int>(vk::ImageUsageFlagBits::eColorAttachment),
  depth_attachment =
      static_cast<int>(vk::ImageUsageFlagBits::eDepthStencilAttachment),
};

using Image_usage_flags = Flags<Image_usage_flag_bits>;

constexpr Image_usage_flags operator|(Image_usage_flag_bits lhs,
                                      Image_usage_flag_bits rhs) noexcept {
  return Image_usage_flags{lhs} | rhs;
}
} // namespace fpsparty::graphics

#endif
