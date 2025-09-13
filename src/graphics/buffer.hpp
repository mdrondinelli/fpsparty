#ifndef FPSPARTY_GRAPHICS_BUFFER_HPP
#define FPSPARTY_GRAPHICS_BUFFER_HPP

#include "vma.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Buffer;

namespace detail {
struct Buffer_create_info {
  vk::BufferCreateInfo buffer_info{};
  vma::Allocation_create_info allocation_info{};
};

constexpr vk::Buffer get_buffer_vk_buffer(const Buffer &buffer) noexcept;

constexpr vma::Allocation get_buffer_vma_allocation(const Buffer &buffer
) noexcept;
} // namespace detail

class Buffer {
public:
  explicit Buffer(const detail::Buffer_create_info &info);

  virtual ~Buffer() = default;

private:
  friend constexpr vk::Buffer detail::get_buffer_vk_buffer(const Buffer &buffer
  ) noexcept;

  friend constexpr vma::Allocation
  detail::get_buffer_vma_allocation(const Buffer &buffer) noexcept;

  vk::UniqueBuffer _vk_buffer{};
  vma::Unique_allocation _vma_allocation{};
};

namespace detail {
constexpr vk::Buffer get_buffer_vk_buffer(const Buffer &buffer) noexcept {
  return *buffer._vk_buffer;
}

constexpr vma::Allocation get_buffer_vma_allocation(const Buffer &buffer
) noexcept {
  return *buffer._vma_allocation;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
