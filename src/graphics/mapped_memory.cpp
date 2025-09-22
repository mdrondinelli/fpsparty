#include "mapped_memory.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "vma.hpp"
#include <tuple>
#include <utility>

namespace fpsparty::graphics {
namespace detail {
Mapped_memory create_mapped_memory(vma::Allocation allocation) {
  return Mapped_memory{allocation};
}
} // namespace detail

Mapped_memory::Mapped_memory(const Mapped_memory &other) noexcept
    : _allocation{other._allocation}, _data{other._data} {
  if (_allocation) {
    std::ignore =
      Global_vulkan_state::get().allocator().map_memory(_allocation);
  }
}

Mapped_memory &Mapped_memory::operator=(const Mapped_memory &other) noexcept {
  auto temp = other;
  swap(temp);
  return *this;
}

Mapped_memory::Mapped_memory(Mapped_memory &&other) noexcept
    : _allocation{std::exchange(other._allocation, {})},
      _data{std::exchange(other._data, {})} {}

Mapped_memory &Mapped_memory::operator=(Mapped_memory &&other) noexcept {
  auto temp = std::move(other);
  swap(temp);
  return *this;
}

Mapped_memory::~Mapped_memory() {
  if (_allocation) {
    Global_vulkan_state::get().allocator().unmap_memory(_allocation);
  }
}

Mapped_memory::operator bool() const noexcept { return _allocation; }

std::span<std::byte> Mapped_memory::get() const noexcept { return _data; }

Mapped_memory::Mapped_memory(vma::Allocation allocation)
    : _allocation{allocation} {
  if (_allocation) {
    const auto allocator = Global_vulkan_state::get().allocator();
    const auto data = allocator.map_memory(_allocation);
    const auto size = allocator.get_allocation_info(_allocation).size;
    _data = {static_cast<std::byte *>(data), size};
  }
}

void Mapped_memory::swap(Mapped_memory &other) noexcept {
  std::swap(_allocation, other._allocation);
  std::swap(_data, other._data);
}
} // namespace fpsparty::graphics
