#include "index_buffer.hpp"
#include "staging_buffer.hpp"
#include <cstdint>
#include <limits>
#include <span>

namespace fpsparty::client {
Index_buffer::Index_buffer(vma::Allocator allocator, vk::Device device,
                           vk::Queue queue, vk::CommandPool command_pool,
                           std::span<const std::byte> data) {
  const auto staging_buffer = Staging_buffer{allocator, data};
  std::tie(_buffer, _allocation) = allocator.create_buffer_unique(
      {.size = static_cast<vk::DeviceSize>(data.size()),
       .usage = vk::BufferUsageFlagBits::eTransferDst |
                vk::BufferUsageFlagBits::eIndexBuffer,
       .sharingMode = vk::SharingMode::eExclusive},
      {.flags = {},
       .usage = c_repr(vma::Memory_usage::e_auto),
       .requiredFlags = {},
       .preferredFlags = {},
       .memoryTypeBits = {},
       .pool = {},
       .pUserData = {},
       .priority = {}});
  const auto command_buffer = std::move(device.allocateCommandBuffersUnique(
      {.commandPool = command_pool,
       .level = vk::CommandBufferLevel::ePrimary,
       .commandBufferCount = 1})[0]);
  command_buffer->begin(
      {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  command_buffer->copyBuffer(
      staging_buffer.get_buffer(), *_buffer,
      {{.srcOffset = 0,
        .dstOffset = 0,
        .size = static_cast<vk::DeviceSize>(data.size())}});
  command_buffer->end();
  const auto fence = device.createFenceUnique({});
  queue.submit({{.commandBufferCount = 1, .pCommandBuffers = &*command_buffer}},
               *fence);
  std::ignore = device.waitForFences({*fence}, vk::True,
                                     std::numeric_limits<std::uint64_t>::max());
}
} // namespace fpsparty::client
