#include "buffer.hpp"
#include "graphics/global_vulkan_state.hpp"

namespace fpsparty::graphics {
Buffer::Buffer(const Buffer_create_info &info) {
  std::tie(_buffer, _allocation) =
      Global_vulkan_state::get().allocator().create_buffer_unique(
          info.buffer_info, info.allocation_info);
}

void Buffer::finalize() noexcept {
  _buffer = {};
  _allocation = {};
}

vk::Buffer Buffer::get_buffer() const noexcept { return *_buffer; }

vma::Allocation Buffer::get_allocation() const noexcept { return *_allocation; }
} // namespace fpsparty::graphics
