#ifndef FPSPARTY_GRAPHICS_DESCRIPTOR_HEAP_HPP
#define FPSPARTY_GRAPHICS_DESCRIPTOR_HEAP_HPP

#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

#include <rc.hpp>

#include "buffer.hpp"
#include "mapped_memory.hpp"

namespace fpsparty::graphics::detail {

struct Descriptor_heap_create_info {
  rc::Factory<Buffer> *buffer_factory;
  std::uint32_t capacity{};
};

class Descriptor_heap {
public:
  constexpr Descriptor_heap() noexcept = default;

  explicit Descriptor_heap(Descriptor_heap_create_info const &info);

  Descriptor_heap(Descriptor_heap const &other) = delete;

  Descriptor_heap &operator=(Descriptor_heap const &other) = delete;

  rc::Strong<Buffer> const &buffer() const noexcept { return _buffer; }

  std::byte *data() noexcept;

  std::uint32_t alloc();

  void free(std::uint32_t index) noexcept;

  void write(std::uint32_t index, std::span<std::byte const> descriptor);

private:
  rc::Strong<Buffer> _buffer{};
  Mapped_memory _memory{};
  std::vector<std::uint32_t> _free_list{};
  std::mutex _mutex;
};

} // namespace fpsparty::graphics::detail

#endif
