#ifndef FPSPARTY_GRAPHICS_IMAGE_HPP
#define FPSPARTY_GRAPHICS_IMAGE_HPP

#include "graphics/image_format.hpp"
#include "rc.hpp"
#include "vma.hpp"
#include <Eigen/Dense>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Image;

namespace detail {
struct Application_image_create_info {
  vk::ImageCreateInfo image_info;
  vma::Allocation_create_info allocation_info;
};

struct External_image_create_info {
  vk::Image image;
  vk::ImageView image_view;
  Image_format format;
  Eigen::Vector3i extent;
  std::uint32_t mip_level_count;
  std::uint32_t array_layer_count;
};

constexpr vk::Image get_image_vk_image(const Image &image) noexcept;

constexpr vk::ImageView get_image_vk_image_view(const Image &image) noexcept;

constexpr vma::Allocation get_image_vma_allocation(const Image &image) noexcept;
} // namespace detail

class Image {
public:
  ~Image();

  Image_format get_format() const noexcept { return _format; }

  const Eigen::Vector3i &get_extent() const noexcept { return _extent; }

  std::uint32_t get_mip_level_count() const noexcept {
    return _mip_level_count;
  }

  std::uint32_t get_array_layer_count() const noexcept {
    return _array_layer_count;
  }

private:
  friend class rc::Factory<Image>;

  friend constexpr vk::Image
  detail::get_image_vk_image(const Image &image) noexcept;

  friend constexpr vk::ImageView
  detail::get_image_vk_image_view(const Image &image) noexcept;

  friend constexpr vma::Allocation
  detail::get_image_vma_allocation(const Image &image) noexcept;

  explicit Image(const detail::Application_image_create_info &info);

  explicit Image(const detail::External_image_create_info &info);

  vk::Image _vk_image{};
  vk::ImageView _vk_image_view{};
  vma::Unique_allocation _vma_allocation{};
  Image_format _format{};
  Eigen::Vector3i _extent{};
  std::uint32_t _mip_level_count{};
  std::uint32_t _array_layer_count{};
};

namespace detail {
constexpr vk::Image get_image_vk_image(const Image &image) noexcept {
  return image._vk_image;
}

constexpr vk::ImageView get_image_vk_image_view(const Image &image) noexcept {
  return image._vk_image_view;
}

constexpr vma::Allocation
get_image_vma_allocation(const Image &image) noexcept {
  return *image._vma_allocation;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
