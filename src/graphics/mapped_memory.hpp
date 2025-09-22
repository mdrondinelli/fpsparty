#ifndef FPSPARTY_GRAPHICS_MAPPED_MEMORY_HPP
#define FPSPARTY_GRAPHICS_MAPPED_MEMORY_HPP

#include "vma.hpp"
#include <cstddef>
#include <span>

namespace fpsparty::graphics {
class Mapped_memory;

namespace detail {
Mapped_memory create_mapped_memory(vma::Allocation allocation);
}

class Mapped_memory {
public:
  Mapped_memory(const Mapped_memory &other) noexcept;

  Mapped_memory &operator=(const Mapped_memory &other) noexcept;

  Mapped_memory(Mapped_memory &&other) noexcept;

  Mapped_memory &operator=(Mapped_memory &&other) noexcept;

  ~Mapped_memory();

  operator bool() const noexcept;

  std::span<std::byte> get() const noexcept;

private:
  friend Mapped_memory detail::create_mapped_memory(vma::Allocation allocation);

  explicit Mapped_memory(vma::Allocation allocation);

  void swap(Mapped_memory &other) noexcept;

  vma::Allocation _allocation{};
  std::span<std::byte> _data{};
};
} // namespace fpsparty::graphics

#endif
