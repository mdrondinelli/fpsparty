#include "buffer.hpp"
#include "graphics/global_vulkan_state.hpp"

namespace fpsparty::graphics {
Buffer::Buffer(const detail::Buffer_create_info &info) {
  std::tie(_vk_buffer, _vma_allocation) =
    Global_vulkan_state::get().allocator().create_buffer_unique(
      info.buffer_info, info.allocation_info
    );
}
} // namespace fpsparty::graphics
