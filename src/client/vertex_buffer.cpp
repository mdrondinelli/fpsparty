#include "vertex_buffer.hpp"
#include "client/global_vulkan_state.hpp"
#include "client/staging_buffer.hpp"
#include <span>

namespace fpsparty::client {
Vertex_buffer::Vertex_buffer(std::size_t size)
    : Graphics_buffer{{
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
      }} {
  /*
    std::tie(_buffer, _allocation) =
        Global_vulkan_state::get().allocator().create_buffer_unique();
    _upload_state.staging_buffer = Staging_buffer{data};
    _upload_state.command_buffer = std::move(
        Global_vulkan_state::get().device().allocateCommandBuffersUnique({
            .commandPool = command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        })[0]);
    _upload_state.command_buffer->begin({
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    _upload_state.command_buffer->copyBuffer(
        _upload_state.staging_buffer.get_buffer(), *_buffer,
        {{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = static_cast<vk::DeviceSize>(data.size()),
        }});
    _upload_state.command_buffer->end();
    _upload_state.fence =
        Global_vulkan_state::get().device().createFenceUnique({});
    Global_vulkan_state::get().queue().submit(
        {{
            .commandBufferCount = 1,
            .pCommandBuffers = &*_upload_state.command_buffer,
        }},
        *_upload_state.fence);
        */
}
/*
Vertex_buffer::Vertex_buffer(Vertex_buffer &&other) noexcept
    : _buffer{std::move(other._buffer)},
      _allocation{std::move(other._allocation)} {
  _upload_state.staging_buffer = std::move(other._upload_state.staging_buffer);
  _upload_state.command_buffer = std::move(other._upload_state.command_buffer);
  _upload_state.fence = std::move(other._upload_state.fence);
}

bool Vertex_buffer::is_uploaded() const {
  if (!_buffer) {
    // maybe a bit paranoid to check this, but this handles the case where
    // `*this` is moved-from
    return false;
  }
  const auto lock = std::scoped_lock{_upload_state.mutex};
  if (!_upload_state.fence) {
    return true;
  } else {
    const auto status = Global_vulkan_state::get().device().getFenceStatus(
        *_upload_state.fence);
    if (status == vk::Result::eSuccess) {
      _upload_state.staging_buffer = {};
      _upload_state.command_buffer = {};
      _upload_state.fence = {};
      return true;
    } else {
      return false;
    }
  }
}
*/
} // namespace fpsparty::client
