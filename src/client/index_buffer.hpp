#ifndef FPSPARTY_CLIENT_INDEX_BUFFER_HPP
#define FPSPARTY_CLIENT_INDEX_BUFFER_HPP

#include "vma.hpp"
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::client {
class Index_buffer {
public:
  constexpr Index_buffer() noexcept = default;

  explicit Index_buffer(vma::Allocator allocator, vk::CommandPool command_pool,
                        std::span<const std::byte> data);

  constexpr vk::Buffer get_buffer() const noexcept { return *_buffer; }

private:
  vk::UniqueBuffer _buffer;
  vma::Unique_allocation _allocation;
};
} // namespace fpsparty::client

#endif
