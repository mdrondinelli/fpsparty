#ifndef FPSPARTY_GRAPHICS_BUFFER_USAGE_HPP
#define FPSPARTY_GRAPHICS_BUFFER_USAGE_HPP

#include "flags.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Buffer_usage_flag_bits {
  transfer_src = static_cast<int>(vk::BufferUsageFlagBits::eTransferSrc),
  transfer_dst = static_cast<int>(vk::BufferUsageFlagBits::eTransferDst),
  index_buffer = static_cast<int>(vk::BufferUsageFlagBits::eIndexBuffer),
  vertex_buffer = static_cast<int>(vk::BufferUsageFlagBits::eVertexBuffer),
  indirect_buffer = static_cast<int>(vk::BufferUsageFlagBits::eIndirectBuffer),
};

using Buffer_usage_flags = Flags<Buffer_usage_flag_bits>;

constexpr Buffer_usage_flags
operator|(Buffer_usage_flag_bits lhs, Buffer_usage_flag_bits rhs) noexcept {
  return Buffer_usage_flags{lhs} | rhs;
}
} // namespace fpsparty::graphics

#endif
