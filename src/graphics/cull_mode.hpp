#ifndef FPSPARTY_GRAPHICS_CULL_MODE_HPP
#define FPSPARTY_GRAPHICS_CULL_MODE_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Cull_mode {
  none = static_cast<int>(vk::CullModeFlagBits::eNone),
  front = static_cast<int>(vk::CullModeFlagBits::eFront),
  back = static_cast<int>(vk::CullModeFlagBits::eBack),
};
}

#endif
