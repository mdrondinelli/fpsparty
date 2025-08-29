#include "image.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/image_format.hpp"
#include <cassert>

namespace fpsparty::graphics {
Image::Image(const Image_create_info &info) {
  assert(info.dimensionality >= 1 && info.dimensionality <= 3);
  assert(info.extent.x() > 0);
  assert(info.extent.y() > 0);
  assert(info.extent.z() > 0);
  const auto image_type = info.dimensionality == 1   ? vk::ImageType::e1D
                          : info.dimensionality == 2 ? vk::ImageType::e2D
                                                     : vk::ImageType::e3D;
  auto [vk_image, vma_allocation] =
      Global_vulkan_state::get().allocator().create_image_unique(
          {
              .imageType = image_type,
              .format = static_cast<vk::Format>(info.format),
              .extent =
                  {
                      .width = static_cast<std::uint32_t>(info.extent.x()),
                      .height = static_cast<std::uint32_t>(info.extent.y()),
                      .depth = static_cast<std::uint32_t>(info.extent.z()),
                  },
              .mipLevels = static_cast<std::uint32_t>(info.mip_level_count),
              .arrayLayers = static_cast<std::uint32_t>(info.array_layer_count),
              .samples = vk::SampleCountFlagBits::e1,
              .tiling = vk::ImageTiling::eOptimal,
              .usage = static_cast<vk::ImageUsageFlags>(info.usage),
              .sharingMode = vk::SharingMode::eExclusive,
              .initialLayout = vk::ImageLayout::eUndefined,
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
  _vk_image = vk_image.release();
  _vk_image_view = Global_vulkan_state::get().device().createImageView({
      .image = _vk_image,
      .viewType = static_cast<vk::ImageViewType>(image_type),
      .format = static_cast<vk::Format>(info.format),
      .subresourceRange =
          {
              .aspectMask =
                  detail::get_image_format_vk_image_aspect_flags(info.format),
              .levelCount = static_cast<std::uint32_t>(info.mip_level_count),
              .layerCount = static_cast<std::uint32_t>(info.array_layer_count),
          },
  });
  _vma_allocation = std::move(vma_allocation);
  _format = info.format;
  _extent = info.extent;
  _mip_level_count = info.mip_level_count;
  _array_layer_count = info.array_layer_count;
}

Image::Image(const detail::External_image_create_info &info)
    : _vk_image{info.image},
      _vk_image_view{info.image_view},
      _format{static_cast<Image_format>(info.format)},
      _extent{info.extent},
      _mip_level_count{info.mip_level_count},
      _array_layer_count{info.array_layer_count} {
  assert(_vk_image);
  assert(_vk_image_view);
}

Image::~Image() {
  if (_vma_allocation) {
    Global_vulkan_state::get().device().destroyImageView(_vk_image_view);
    Global_vulkan_state::get().device().destroyImage(_vk_image);
  }
}
} // namespace fpsparty::graphics
