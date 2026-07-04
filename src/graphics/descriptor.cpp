#include "descriptor.hpp"

#include <cassert>

#include "descriptor_heap.hpp"

namespace fpsparty::graphics {

Descriptor::Descriptor(detail::Descriptor_create_info info) noexcept
    : _heap{info.heap},
      _image{std::move(info.image)},
      _type{info.type},
      _handle{info.handle} {}

Descriptor::~Descriptor() {
  switch (_type) {
  case Descriptor_type::sampled_image:
    _heap->free_sampled_image(_handle);
    break;
  case Descriptor_type::storage_image:
    _heap->free_storage_image(_handle);
    break;
  default:
    assert(false);
    std::unreachable();
  }
}

} // namespace fpsparty::graphics
