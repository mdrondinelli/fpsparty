#include "descriptor_heap_pool.hpp"

namespace fpsparty::graphics::detail {
Descriptor_heap_pool::Descriptor_heap_pool(
  Descriptor_heap_pool_create_info const &info)
    : _buffer_factory{info.buffer_factory}, _buffers{} {}

rc::Strong<Buffer> Descriptor_heap_pool::pop() {
  auto lock = std::unique_lock{_mutex};
  if (!_buffers.empty()) {
    auto buffer = std::move(_buffers.back());
    _buffers.pop_back();
    return buffer;
  } else {
    lock.unlock();
    // TODO: make descriptor heaps grow
    // future strategy:
    // allocate new larger buffer, copy old buffer to new, rebind
    return _buffer_factory->create(
      Buffer_create_info{
        .size = 4096,
        .usage = Buffer_usage_flag_bits::shader_device_address |
                 Buffer_usage_flag_bits::descriptor_heap,
        .mapping_mode = Mapping_mode::write_only,
      });
  }
}

} // namespace fpsparty::graphics::detail
