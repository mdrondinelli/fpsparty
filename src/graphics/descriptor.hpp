#ifndef FPSPARTY_GRAPHICS_DESCRIPTOR_HPP
#define FPSPARTY_GRAPHICS_DESCRIPTOR_HPP

#include <int.hpp>
#include <rc.hpp>

#include "image.hpp"

namespace fpsparty::graphics {
class Descriptor;

namespace detail {
class Descriptor_heap;

struct Descriptor_create_info {
  rc::Weak<Descriptor_heap> heap;
  rc::Strong<Image const> image;
  u32 handle{};
};
} // namespace detail

class Descriptor {
public:
  ~Descriptor();

  u32 get_handle() const noexcept { return _handle; }

private:
  friend class rc::Factory<Descriptor>;

  explicit Descriptor(detail::Descriptor_create_info info) noexcept;

  rc::Weak<detail::Descriptor_heap> _heap{};
  rc::Strong<Image const> _image{};
  u32 _handle{};
};
} // namespace fpsparty::graphics

#endif
