#include "staging_buffer.hpp"
#include "graphics/global_vulkan_state.hpp"
#include <cstring>

namespace fpsparty::graphics {
Staging_buffer::Staging_buffer(std::span<const std::byte> data)
    : Buffer{{
        .buffer_info =
          {
            .size = static_cast<vk::DeviceSize>(data.size()),
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive,
          },
        .allocation_info =
          {
            .flags = c_repr(
              vma::Allocation_create_flag_bits::e_host_access_sequential_write),
            .usage = c_repr(vma::Memory_usage::e_auto),
            .requiredFlags = static_cast<VkMemoryPropertyFlags>(
              vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent),
            .preferredFlags = {},
            .memoryTypeBits = {},
            .pool = {},
            .pUserData = {},
            .priority = {},
          },
      }} {
  if (!data.empty()) {
    const auto allocation = detail::get_buffer_vma_allocation(*this);
    void *mapped_memory =
      Global_vulkan_state::get().allocator().map_memory(allocation);
    std::memcpy(mapped_memory, data.data(), data.size());
    Global_vulkan_state::get().allocator().unmap_memory(allocation);
  }
}
} // namespace fpsparty::graphics
