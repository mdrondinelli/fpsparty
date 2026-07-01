#include "descriptor_heap.hpp"
#include "buffer_usage.hpp"
#include "global_vulkan_state.hpp"
#include <cassert>
#include <cstring>
#include <numeric>
#include <stdexcept>

namespace fpsparty::graphics::detail {

namespace {

constexpr std::uint64_t
align_up(std::uint64_t value, std::uint64_t alignment) noexcept {
  return (value + alignment - 1) / alignment * alignment;
}

} // namespace

Descriptor_heap::Descriptor_heap(Descriptor_heap_create_info const &info) : _buffer{[&]{
  auto const &properties = Global_vulkan_state::get().descriptor_heap_properties();
  assert(properties.imageDescriptorSize % properties.imageDescriptorAlignment == 0);
  auto const descriptor_alignment = std::lcm(
    properties.imageDescriptorAlignment, properties.bufferDescriptorAlignment);
  auto const reserved_range_offset = align_up(
    static_cast<std::uint64_t>(info.capacity) *
      properties.imageDescriptorSize,
    descriptor_alignment);
  return info.buffer_factory->create(
    Buffer_create_info{
      .size = reserved_range_offset + properties.minResourceHeapReservedRange,
      .usage = Buffer_usage_flag_bits::shader_device_address |
               Buffer_usage_flag_bits::descriptor_heap,
      .mapping_mode = Mapping_mode::write_only,
      .min_alignment = properties.resourceHeapAlignment,
    });
}()}, _memory{_buffer->map()} {
  _free_list.reserve(info.capacity);
  for (auto i = info.capacity; i != 0; --i) {
    _free_list.push_back(i - 1);
  }
}

std::byte *Descriptor_heap::data() noexcept {
  return _memory.get().data();
}

std::uint32_t Descriptor_heap::alloc() {
  auto const lock = std::scoped_lock{_mutex};
  if (_free_list.empty()) {
    throw std::runtime_error{"Descriptor heap is out of space"};
  }
  auto const retval = _free_list.back();
  _free_list.pop_back();
  return retval;
}

void Descriptor_heap::free(std::uint32_t index) noexcept {
  auto const lock = std::scoped_lock{_mutex};
  _free_list.push_back(index);
}

void Descriptor_heap::write(
  std::uint32_t index, std::span<std::byte const> descriptor) {
  auto const descriptor_size =
    Global_vulkan_state::get().descriptor_heap_properties().imageDescriptorSize;
  assert(descriptor.size() == descriptor_size);
  std::memcpy(
    data() + static_cast<std::size_t>(index) * descriptor_size,
    descriptor.data(),
    descriptor.size());
}

} // namespace fpsparty::graphics::detail
