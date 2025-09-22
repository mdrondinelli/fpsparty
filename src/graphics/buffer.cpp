#include "buffer.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/mapped_memory.hpp"
#include "vma.hpp"
#include <utility>

namespace fpsparty::graphics {
Buffer::Buffer(const Buffer_create_info &info) {
  const auto buffer_info = vk::BufferCreateInfo{
    .size = info.size,
    .usage = static_cast<vk::BufferUsageFlags>(info.usage),
    .sharingMode = vk::SharingMode::eExclusive,
  };
  const auto [allocation_flags, memory_flags] =
    [&]() -> std::pair<vma::Allocation_create_flags, vk::MemoryPropertyFlags> {
    switch (info.mapping_mode) {
    case Mapping_mode::write_only:
      return {
        vma::Allocation_create_flag_bits::e_host_access_sequential_write,
        vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent,
      };
    case Mapping_mode::read_write:
      return {
        vma::Allocation_create_flag_bits::e_host_access_random,
        vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent,
      };
    default:
      return {{}, {}};
    }
  }();
  const auto allocation_info = vma::Allocation_create_info{
    .flags = c_repr(allocation_flags),
    .usage = c_repr(vma::Memory_usage::e_auto),
    .requiredFlags = static_cast<VkMemoryPropertyFlags>(memory_flags),
    .preferredFlags = {},
    .memoryTypeBits = {},
    .pool = {},
    .pUserData = {},
    .priority = {},
  };
  std::tie(_vk_buffer, _vma_allocation) =
    Global_vulkan_state::get().allocator().create_buffer_unique(
      buffer_info, allocation_info);
}

Mapped_memory Buffer::map() {
  return detail::create_mapped_memory(*_vma_allocation);
}
} // namespace fpsparty::graphics
