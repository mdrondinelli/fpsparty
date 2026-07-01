#include "descriptor.hpp"

#include "descriptor_heap.hpp"

namespace fpsparty::graphics {

Descriptor::Descriptor(detail::Descriptor_create_info info) noexcept
    : _heap{std::move(info.heap)},
      _image{std::move(info.image)},
      _handle{info.handle} {}

Descriptor::~Descriptor() {
  if (auto heap = _heap.lock()) {
    heap->free(_handle);
  }
}
} // namespace fpsparty::graphics
