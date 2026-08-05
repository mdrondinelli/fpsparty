#ifndef FPSPARTY_GRAPHICS_IMAGE_HPP
#define FPSPARTY_GRAPHICS_IMAGE_HPP

#include "graphics/image_format.hpp"
#include "graphics/image_usage.hpp"
#include "int.hpp"
#include "rc.hpp"
#include "vma.hpp"
#include <Eigen/Dense>
#include <cassert>
#include <optional>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Image;

namespace detail {
class Descriptor_heap;

struct External_image_create_info {
  vk::Image image;
  vk::ImageView image_view;
  Image_format format;
  Eigen::Vector3i extent;
  int mip_level_count;
  int array_layer_count;
};

constexpr vk::Image get_image_vk_image(Image const &image) noexcept;

constexpr vk::ImageView get_image_vk_image_view(Image const &image) noexcept;

constexpr vma::Allocation get_image_vma_allocation(Image const &image) noexcept;
} // namespace detail

struct Image_create_info {
  int dimensionality;
  Image_format format;
  Eigen::Vector3i extent;
  int mip_level_count;
  int array_layer_count;
  Image_usage_flags usage;
};

// Sampled/storage descriptors (per Image_create_info::usage) are
// allocated and made resident in the bindless descriptor heap at
// construction, and freed at destruction.
class Image {
public:
  explicit Image(Image_create_info const &info);

  explicit Image(
    Image_create_info const &info, detail::Descriptor_heap &descriptor_heap);

  ~Image();

  Image_format get_format() const noexcept { return _format; }

  Eigen::Vector3i const &get_extent() const noexcept { return _extent; }

  int get_mip_level_count() const noexcept { return _mip_level_count; }

  int get_array_layer_count() const noexcept { return _array_layer_count; }

  u32 get_sampled_descriptor_index() const noexcept {
    assert(_sampled_descriptor_handle);
    return *_sampled_descriptor_handle;
  }

  u32 get_storage_descriptor_index() const noexcept {
    assert(_storage_descriptor_handle);
    return *_storage_descriptor_handle;
  }

private:
  friend class rc::Factory<Image>;

  friend constexpr vk::Image
  detail::get_image_vk_image(Image const &image) noexcept;

  friend constexpr vk::ImageView
  detail::get_image_vk_image_view(Image const &image) noexcept;

  friend constexpr vma::Allocation
  detail::get_image_vma_allocation(Image const &image) noexcept;

  explicit Image(detail::External_image_create_info const &info);

  void allocate_descriptors(
    detail::Descriptor_heap &descriptor_heap, Image_usage_flags usage);

  vma::Unique_allocation _vma_allocation{};
  vk::Image _vk_image{};
  vk::ImageView _vk_image_view{};
  Image_format _format{};
  Eigen::Vector3i _extent{};
  int _mip_level_count{};
  int _array_layer_count{};
  detail::Descriptor_heap *_descriptor_heap{};
  std::optional<u32> _sampled_descriptor_handle{};
  std::optional<u32> _storage_descriptor_handle{};
};

namespace detail {
constexpr vk::Image get_image_vk_image(Image const &image) noexcept {
  return image._vk_image;
}

constexpr vk::ImageView get_image_vk_image_view(Image const &image) noexcept {
  return image._vk_image_view;
}

constexpr vma::Allocation
get_image_vma_allocation(Image const &image) noexcept {
  return *image._vma_allocation;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
