#ifndef FPSPARTY_GRAPHICS_IMAGE_LAYOUT_HPP
#define FPSPARTY_GRAPHICS_IMAGE_LAYOUT_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Image_layout {
  undefined = static_cast<int>(vk::ImageLayout::eUndefined),
  general = static_cast<int>(vk::ImageLayout::eGeneral),
  present_src = static_cast<int>(vk::ImageLayout::ePresentSrcKHR),
};
}

#endif
