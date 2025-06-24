#ifndef FPSPARTY_CLIENT_STAGING_BUFFER_HPP
#define FPSPARTY_CLIENT_STAGING_BUFFER_HPP

#include "vma.hpp"
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::client {
class Staging_buffer {
public:
  constexpr Staging_buffer() noexcept = default;

  explicit Staging_buffer(std::span<const std::byte> data);

  vk::Buffer get_buffer() const { return *_buffer; }

private:
  vk::UniqueBuffer _buffer{};
  vma::Unique_allocation _allocation{};
};
} // namespace fpsparty::client

#endif
