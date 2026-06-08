#ifndef FPSPARTY_GRAPHICS_DESCRIPTOR_HEAP_POOL_HPP
#define FPSPARTY_GRAPHICS_DESCRIPTOR_HEAP_POOL_HPP

#include <mutex>

#include <rc.hpp>

#include "buffer.hpp"

namespace fpsparty::graphics::detail {
struct Descriptor_heap_pool_create_info {
  rc::Factory<Buffer> *buffer_factory;
};

class Descriptor_heap_pool {
public:
  constexpr Descriptor_heap_pool() noexcept = default;

  explicit Descriptor_heap_pool(Descriptor_heap_pool_create_info const &info);

  rc::Strong<Buffer> pop();

  void push(rc::Strong<Buffer> buffer);

private:
  rc::Factory<Buffer> *_buffer_factory{};
  std::vector<rc::Strong<Buffer>> _buffers;
  std::mutex _mutex;
};

} // namespace fpsparty::graphics::detail

#endif
