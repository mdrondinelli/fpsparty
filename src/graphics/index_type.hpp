#ifndef FPSPARTY_GRAPHICS_INDEX_TYPE_HPP
#define FPSPARTY_GRAPHICS_INDEX_TYPE_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Index_type {
  u16 = static_cast<std::underlying_type_t<vk::IndexType>>(
      vk::IndexType::eUint16),
  u32 = static_cast<std::underlying_type_t<vk::IndexType>>(
      vk::IndexType::eUint32),
};
}

#endif
