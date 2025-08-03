#ifndef FPSPARTY_CLIENT_GRAPHICS_BUFFER_HPP
#define FPSPARTY_CLIENT_GRAPHICS_BUFFER_HPP

#include "rc.hpp"
#include "vma.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
struct Graphics_buffer_create_info {
  vk::BufferCreateInfo buffer_info{};
  vma::Allocation_create_info allocation_info{};
};

class Graphics_buffer : public rc::Object<Graphics_buffer> {
public:
  explicit Graphics_buffer(const Graphics_buffer_create_info &info);

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
