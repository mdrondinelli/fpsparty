#include "vertex_buffer.hpp"
#include "vma.hpp"
#include <cstdint>
#include <limits>

namespace fpsparty::client {
Vertex_buffer::Vertex_buffer(vma::Allocator allocator, vk::Device device,
                             vk::Queue queue, vk::CommandPool command_pool,
                             const void *data, std::size_t size) {
  const auto [staging_buffer, staging_allocation] =
      allocator.create_buffer_unique(
          {.size = static_cast<vk::DeviceSize>(size),
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
  std::tie(_buffer, _allocation) = allocator.create_buffer_unique(
      {.size = static_cast<vk::DeviceSize>(size),
       .usage = vk::BufferUsageFlagBits::eTransferDst |
                vk::BufferUsageFlagBits::eVertexBuffer,
       .sharingMode = vk::SharingMode::eExclusive},
      {.flags = {},
       .usage = c_repr(vma::Memory_usage::e_auto),
       .requiredFlags = {},
       .preferredFlags = {},
       .memoryTypeBits = {},
       .pool = {},
       .pUserData = {},
       .priority = {}});
  const auto staging_memory = allocator.map_memory(*staging_allocation);
  std::memcpy(staging_memory, data, size);
  allocator.unmap_memory(*staging_allocation);
  const auto command_buffer = std::move(device.allocateCommandBuffersUnique(
      {.commandPool = command_pool,
       .level = vk::CommandBufferLevel::ePrimary,
       .commandBufferCount = 1})[0]);
  command_buffer->begin(
      {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  command_buffer->copyBuffer(*staging_buffer, *_buffer,
                             {{.srcOffset = 0,
                               .dstOffset = 0,
                               .size = static_cast<vk::DeviceSize>(size)}});
  command_buffer->end();
  const auto fence = device.createFenceUnique({});
  queue.submit({{.commandBufferCount = 1, .pCommandBuffers = &*command_buffer}},
               *fence);
  std::ignore = device.waitForFences({*fence}, vk::True,
                                     std::numeric_limits<std::uint64_t>::max());
}
} // namespace fpsparty::client
