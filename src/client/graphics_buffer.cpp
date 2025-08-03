#include "graphics_buffer.hpp"
#include "client/global_vulkan_state.hpp"

namespace fpsparty::client {
Graphics_buffer::Graphics_buffer(const Graphics_buffer_create_info &info) {
  std::tie(_buffer, _allocation) =
      Global_vulkan_state::get().allocator().create_buffer_unique(
          info.buffer_info, info.allocation_info);
}

void Graphics_buffer::finalize() noexcept {
  _buffer = {};
  _allocation = {};
}

vk::Buffer Graphics_buffer::get_buffer() const noexcept { return *_buffer; }

vma::Allocation Graphics_buffer::get_allocation() const noexcept {
  return *_allocation;
}
} // namespace fpsparty::client
