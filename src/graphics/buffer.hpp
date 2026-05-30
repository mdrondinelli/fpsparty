#ifndef FPSPARTY_GRAPHICS_BUFFER_HPP
#define FPSPARTY_GRAPHICS_BUFFER_HPP

#include "graphics/buffer_usage.hpp"
#include "graphics/mapped_memory.hpp"
#include "graphics/mapping_mode.hpp"
#include "vma.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Buffer;

namespace detail {
constexpr vk::Buffer get_buffer_vk_buffer(Buffer const &buffer) noexcept;

constexpr vma::Allocation
get_buffer_vma_allocation(Buffer const &buffer) noexcept;
} // namespace detail

struct Buffer_create_info {
  std::uint64_t size;
  Buffer_usage_flags usage;
  Mapping_mode mapping_mode{Mapping_mode::none};
};

class Buffer {
public:
  explicit Buffer(Buffer_create_info const &info);

  virtual ~Buffer() = default;

  Mapped_memory map();

  std::uint64_t get_device_address() const noexcept;

private:
  friend constexpr vk::Buffer
  detail::get_buffer_vk_buffer(Buffer const &buffer) noexcept;

  friend constexpr vma::Allocation
  detail::get_buffer_vma_allocation(Buffer const &buffer) noexcept;

  vk::UniqueBuffer _vk_buffer{};
  vma::Unique_allocation _vma_allocation{};
};

namespace detail {
constexpr vk::Buffer get_buffer_vk_buffer(Buffer const &buffer) noexcept {
  return *buffer._vk_buffer;
}

constexpr vma::Allocation
get_buffer_vma_allocation(Buffer const &buffer) noexcept {
  return *buffer._vma_allocation;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
