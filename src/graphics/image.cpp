#include "image.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/image_format.hpp"
#include <cassert>

namespace fpsparty::graphics {
Image::Image(Image_create_info const &info) {
  assert(info.dimensionality >= 1 && info.dimensionality <= 3);
  assert(info.extent.x() > 0);
  assert(info.extent.y() > 0);
  assert(info.extent.z() > 0);
  auto const image_type = info.dimensionality == 1   ? vk::ImageType::e1D
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
  _vma_allocation = std::move(vma_allocation);
  auto const image_view_create_info = vk::ImageViewCreateInfo{
    .image = *vk_image,
    .viewType = static_cast<vk::ImageViewType>(image_type),
    .format = static_cast<vk::Format>(info.format),
    .subresourceRange =
      {
        .aspectMask =
          detail::get_image_format_vk_image_aspect_flags(info.format),
        .levelCount = static_cast<std::uint32_t>(info.mip_level_count),
        .layerCount = static_cast<std::uint32_t>(info.array_layer_count),
      },
  };
  auto vk_image_view = Global_vulkan_state::get()
                         .device()
                         .createImageViewUnique(image_view_create_info);
  auto const descriptor_info = vk::ImageDescriptorInfoEXT{
    .pView = &image_view_create_info,
    .layout = vk::ImageLayout::eGeneral,
  };
  if (info.usage & Image_usage_flag_bits::sampled) {
    _sampled_image_descriptor.resize(
      Global_vulkan_state::get()
        .descriptor_heap_properties()
        .imageDescriptorSize);
    Global_vulkan_state::get().device().writeResourceDescriptorsEXT(
      {
        vk::ResourceDescriptorInfoEXT{
          .type = vk::DescriptorType::eSampledImage,
          .data = &descriptor_info,
        },
      },
      {
        vk::HostAddressRangeEXT{
          .address = _sampled_image_descriptor.data(),
          .size = _sampled_image_descriptor.size(),
        },
      });
  }
  if (info.usage & Image_usage_flag_bits::storage) {
    _storage_image_descriptor.resize(
      Global_vulkan_state::get()
        .descriptor_heap_properties()
        .imageDescriptorSize);
    Global_vulkan_state::get().device().writeResourceDescriptorsEXT(
      {
        vk::ResourceDescriptorInfoEXT{
          .type = vk::DescriptorType::eStorageImage,
          .data = &descriptor_info,
        },
      },
      {
        vk::HostAddressRangeEXT{
          .address = _storage_image_descriptor.data(),
          .size = _storage_image_descriptor.size(),
        },
      });
  }
  _vk_image = vk_image.release();
  _vk_image_view = vk_image_view.release();
  _format = info.format;
  _extent = info.extent;
  _mip_level_count = info.mip_level_count;
  _array_layer_count = info.array_layer_count;
}

Image::Image(detail::External_image_create_info const &info)
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
