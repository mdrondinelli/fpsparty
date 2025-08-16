#ifndef FPSPARTY_GRAPHICS_IMAGE_HPP
#define FPSPARTY_GRAPHICS_IMAGE_HPP

#include "vma.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Image;

namespace detail {
struct Image_create_info {
  vk::Image swapchain_image{};
  vk::ImageCreateInfo image_info;
  vma::Allocation_create_info allocation_info;
};

inline Image create_image(const Image_create_info &info);

constexpr vk::Image get_image_vk_image(const Image &image) noexcept;

constexpr vma::Allocation get_image_vma_allocation(const Image &image) noexcept;
} // namespace detail

class Image {
public:
  ~Image();

private:
  friend inline Image
  detail::create_image(const detail::Image_create_info &info);

  friend constexpr vk::Image
  detail::get_image_vk_image(const Image &image) noexcept;

  friend constexpr vma::Allocation
  detail::get_image_vma_allocation(const Image &image) noexcept;

  explicit Image(const detail::Image_create_info &info);

  vk::Image _vk_image{};
  vma::Unique_allocation _vma_allocation{};
};

namespace detail {
inline Image create_image(const Image_create_info &info) { return Image{info}; }

constexpr vk::Image get_image_vk_image(const Image &image) noexcept {
  return image._vk_image;
}

constexpr vma::Allocation
get_image_vma_allocation(const Image &image) noexcept {
  return *image._vma_allocation;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
