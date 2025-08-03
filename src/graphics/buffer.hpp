#ifndef FPSPARTY_GRAPHICS_BUFFER_HPP
#define FPSPARTY_GRAPHICS_BUFFER_HPP

#include "rc.hpp"
#include "vma.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
struct Buffer_create_info {
  vk::BufferCreateInfo buffer_info{};
  vma::Allocation_create_info allocation_info{};
};

class Buffer : public rc::Object<Buffer> {
public:
  explicit Buffer(const Buffer_create_info &info);

  vk::Buffer get_buffer() const noexcept;

  vma::Allocation get_allocation() const noexcept;

protected:
  void finalize() noexcept override;

private:
  vk::UniqueBuffer _buffer{};
  vma::Unique_allocation _allocation{};
};
} // namespace fpsparty::graphics

#endif
