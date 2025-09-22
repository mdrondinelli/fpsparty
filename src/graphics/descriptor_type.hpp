#ifndef FPSPARTY_GRAPHICS_DESCRIPTOR_TYPE_HPP
#define FPSPARTY_GRAPHICS_DESCRIPTOR_TYPE_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Descriptor_type {
  storage_buffer = static_cast<int>(vk::DescriptorType::eStorageBuffer),
};
}

#endif
