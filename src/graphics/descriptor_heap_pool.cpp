#include "descriptor_heap_pool.hpp"
#include "global_vulkan_state.hpp"
#include <numeric>

namespace fpsparty::graphics::detail {
namespace {
constexpr std::uint64_t align_up(
  std::uint64_t value, std::uint64_t alignment) noexcept {
  return (value + alignment - 1) / alignment * alignment;
}
} // namespace

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
    auto const &properties =
      Global_vulkan_state::get().descriptor_heap_properties();
    auto const descriptor_alignment = std::lcm(
      properties.imageDescriptorAlignment,
      properties.bufferDescriptorAlignment);
    auto const reserved_range_offset =
      align_up(4096, descriptor_alignment);
    // TODO: make descriptor heaps grow
    // future strategy:
    // allocate new larger buffer, copy old buffer to new, rebind
    return _buffer_factory->create(
      Buffer_create_info{
        .size =
          reserved_range_offset + properties.minResourceHeapReservedRange,
        .usage = Buffer_usage_flag_bits::shader_device_address |
                 Buffer_usage_flag_bits::descriptor_heap,
        .mapping_mode = Mapping_mode::write_only,
        .min_alignment = properties.resourceHeapAlignment,
      });
  }
}

void Descriptor_heap_pool::push(rc::Strong<Buffer> buffer) {
  auto const lock = std::scoped_lock{_mutex};
  _buffers.emplace_back(std::move(buffer));
}

} // namespace fpsparty::graphics::detail
