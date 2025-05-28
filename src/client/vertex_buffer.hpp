#ifndef FPSPARTY_CLIENT_VERTEX_BUFFER_HPP
#define FPSPARTY_CLIENT_VERTEX_BUFFER_HPP

#include "vma.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::client {
class Vertex_buffer {
public:
  constexpr Vertex_buffer() noexcept = default;

  explicit Vertex_buffer(vma::Allocator allocator, vk::Device device,
                         vk::Queue queue, vk::CommandPool command_pool,
                         const void *data, std::size_t size);

  constexpr vk::Buffer get_buffer() const noexcept { return *_buffer; }

private:
  vk::UniqueBuffer _buffer;
  vma::Unique_allocation _allocation;
};
} // namespace fpsparty::client

#endif
