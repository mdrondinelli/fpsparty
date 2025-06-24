#include "staging_buffer.hpp"
#include "client/global_vulkan_state.hpp"
#include <cstring>

namespace fpsparty::client {
Staging_buffer::Staging_buffer(std::span<const std::byte> data) {
  std::tie(_buffer, _allocation) =
      Global_vulkan_state::get().allocator().create_buffer_unique(
          {.size = static_cast<vk::DeviceSize>(data.size()),
           .usage = vk::BufferUsageFlagBits::eTransferSrc,
           .sharingMode = vk::SharingMode::eExclusive},
          {.flags = c_repr(vma::Allocation_create_flag_bits::
                               e_host_access_sequential_write),
           .usage = c_repr(vma::Memory_usage::e_auto),
           .requiredFlags = static_cast<VkMemoryPropertyFlags>(
               vk::MemoryPropertyFlagBits::eHostVisible |
               vk::MemoryPropertyFlagBits::eHostCoherent),
           .preferredFlags = {},
           .memoryTypeBits = {},
           .pool = {},
           .pUserData = {},
           .priority = {}});
  if (!data.empty()) {
    void *mapped_memory =
        Global_vulkan_state::get().allocator().map_memory(*_allocation);
    std::memcpy(mapped_memory, data.data(), data.size());
    Global_vulkan_state::get().allocator().unmap_memory(*_allocation);
  }
}
} // namespace fpsparty::client
