#include "vertex_buffer.hpp"
#include "client/global_vulkan_state.hpp"
#include "client/staging_buffer.hpp"
#include <cstdint>
#include <limits>
#include <span>

namespace fpsparty::client {
Vertex_buffer::Vertex_buffer(vk::CommandPool command_pool,
                             std::span<const std::byte> data) {
  const auto staging_buffer = Staging_buffer{data};
  std::tie(_buffer, _allocation) =
      Global_vulkan_state::get().allocator().create_buffer_unique(
          {
              .size = static_cast<vk::DeviceSize>(data.size()),
              .usage = vk::BufferUsageFlagBits::eTransferDst |
                       vk::BufferUsageFlagBits::eVertexBuffer,
              .sharingMode = vk::SharingMode::eExclusive,
          },
          {
              .flags = {},
              .usage = c_repr(vma::Memory_usage::e_auto),
              .requiredFlags = {},
              .preferredFlags = {},
              .memoryTypeBits = {},
              .pool = {},
              .pUserData = {},
              .priority = {},
          });
  const auto command_buffer = std::move(
      Global_vulkan_state::get().device().allocateCommandBuffersUnique({
          .commandPool = command_pool,
          .level = vk::CommandBufferLevel::ePrimary,
          .commandBufferCount = 1,
      })[0]);
  command_buffer->begin({
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  });
  command_buffer->copyBuffer(
      staging_buffer.get_buffer(), *_buffer,
      {{
          .srcOffset = 0,
          .dstOffset = 0,
          .size = static_cast<vk::DeviceSize>(data.size()),
      }});
  command_buffer->end();
  const auto fence = Global_vulkan_state::get().device().createFenceUnique({});
  Global_vulkan_state::get().queue().submit(
      {{
          .commandBufferCount = 1,
          .pCommandBuffers = &*command_buffer,
      }},
      *fence);
  std::ignore = Global_vulkan_state::get().device().waitForFences(
      {*fence}, vk::True, std::numeric_limits<std::uint64_t>::max());
}
} // namespace fpsparty::client
