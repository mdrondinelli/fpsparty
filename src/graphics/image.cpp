#include "image.hpp"
#include "graphics/global_vulkan_state.hpp"
#include <iostream>

namespace fpsparty::graphics {
Image::Image(const detail::Application_image_create_info &info) {
  auto [vk_image, vma_allocation] =
      Global_vulkan_state::get().allocator().create_image_unique(
          info.image_info, info.allocation_info);
  _vk_image = vk_image.release();
  _vk_image_view = Global_vulkan_state::get().device().createImageView({
      .image = _vk_image,
      .viewType = static_cast<vk::ImageViewType>(info.image_info.imageType),
      .format = info.image_info.format,
      .subresourceRange =
          {
              .aspectMask = vk::ImageAspectFlagBits::eColor,
              .levelCount = info.image_info.mipLevels,
              .layerCount = info.image_info.arrayLayers,
          },
  });
  _vma_allocation = std::move(vma_allocation);
  _format = static_cast<Image_format>(info.image_info.format),
  _extent = {
      static_cast<int>(info.image_info.extent.width),
      static_cast<int>(info.image_info.extent.height),
      static_cast<int>(info.image_info.extent.depth),
  };
  _mip_level_count = info.image_info.mipLevels;
  _array_layer_count = info.image_info.arrayLayers;
}

Image::Image(const detail::External_image_create_info &info)
    : _vk_image{info.image},
      _vk_image_view{info.image_view},
      _format{static_cast<Image_format>(info.format)},
      _extent{info.extent},
      _mip_level_count{info.mip_level_count},
      _array_layer_count{info.array_layer_count} {
  if (!_vk_image) {
    std::cerr << "Somehow got null external image.\n";
  }
}

Image::~Image() {
  if (_vma_allocation) {
    Global_vulkan_state::get().device().destroyImageView(_vk_image_view);
    Global_vulkan_state::get().device().destroyImage(_vk_image);
  }
}

Image_format Image::get_format() const noexcept { return _format; }

const Eigen::Vector3i &Image::get_extent() const noexcept { return _extent; }

std::uint32_t Image::get_mip_level_count() const noexcept {
  return _mip_level_count;
}

std::uint32_t Image::get_array_layer_count() const noexcept {
  return _array_layer_count;
}
} // namespace fpsparty::graphics
