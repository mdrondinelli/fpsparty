#include "image.hpp"
#include "graphics/global_vulkan_state.hpp"

namespace fpsparty::graphics {
Image::Image(const detail::Image_create_info &info)
    : _vk_image{info.swapchain_image} {
  if (!_vk_image) {
    auto [vk_image, vma_allocation] =
        Global_vulkan_state::get().allocator().create_image_unique(
            info.image_info, info.allocation_info);
    _vk_image = vk_image.release();
    _vma_allocation = std::move(vma_allocation);
  }
}

Image::~Image() {
  if (_vma_allocation) {
    Global_vulkan_state::get().device().destroyImage(_vk_image);
  }
}
} // namespace fpsparty::graphics
