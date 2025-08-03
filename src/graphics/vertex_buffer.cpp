#include "vertex_buffer.hpp"

namespace fpsparty::graphics {
Vertex_buffer::Vertex_buffer(std::size_t size)
    : Buffer{{
          .buffer_info =
              {
                  .size = static_cast<vk::DeviceSize>(size),
                  .usage = vk::BufferUsageFlagBits::eTransferDst |
                           vk::BufferUsageFlagBits::eVertexBuffer,
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
