#include "descriptor_heap.hpp"
#include "buffer_usage.hpp"
#include "global_vulkan_state.hpp"
#include <algorithm>
#include <cassert>
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
  _free_list.push_back({.offset = 0, .size = info.capacity});
}

std::byte *Descriptor_heap::data() noexcept {
  return _memory.get().data();
}

Descriptor_allocation Descriptor_heap::alloc(std::uint32_t count) {
  auto const lock = std::scoped_lock{_mutex};
  for (auto it = _free_list.begin(); it != _free_list.end(); ++it) {
    if (it->size >= count) {
      auto const offset = it->offset;
      it->offset += count;
      it->size -= count;
      if (it->size == 0) {
        _free_list.erase(it);
      }
      return {.offset = offset, .size = count};
    }
  }
  throw std::runtime_error{"Descriptor heap is out of space"};
}

void Descriptor_heap::free(
  Descriptor_allocation allocation) noexcept {
  if (allocation.size == 0) {
    return;
  }
  auto const lock = std::scoped_lock{_mutex};
  auto const it = std::ranges::upper_bound(
    _free_list,
    allocation.offset,
    {},
    [](Descriptor_allocation const &region) { return region.offset; });
  auto const inserted = _free_list.insert(
    it, {.offset = allocation.offset, .size = allocation.size});
  auto const next = std::next(inserted);
  if (next != _free_list.end() &&
      inserted->offset + inserted->size == next->offset) {
    inserted->size += next->size;
    _free_list.erase(next);
  }
  if (inserted != _free_list.begin()) {
    auto const prev = std::prev(inserted);
    if (prev->offset + prev->size == inserted->offset) {
      prev->size += inserted->size;
      _free_list.erase(inserted);
    }
  }
}

} // namespace fpsparty::graphics::detail
