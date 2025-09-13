#include "index_buffer.hpp"

namespace fpsparty::graphics {
Index_buffer::Index_buffer(std::size_t size)
    : Buffer{{
        .buffer_info =
          {
            .size = static_cast<vk::DeviceSize>(size),
            .usage = vk::BufferUsageFlagBits::eTransferDst |
                     vk::BufferUsageFlagBits::eIndexBuffer,
            .sharingMode = vk::SharingMode::eExclusive,
          },
        .allocation_info =
          {
            .flags = {},
            .usage = c_repr(vma::Memory_usage::e_auto),
            .requiredFlags = {},
            .preferredFlags = {},
            .memoryTypeBits = {},
            .pool = {},
            .pUserData = {},
            .priority = {},
          },
      }} {}
} // namespace fpsparty::graphics
