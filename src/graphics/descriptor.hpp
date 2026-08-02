#ifndef FPSPARTY_GRAPHICS_DESCRIPTOR_HPP
#define FPSPARTY_GRAPHICS_DESCRIPTOR_HPP

#include <int.hpp>
#include <rc.hpp>

#include "descriptor_type.hpp"
#include "image.hpp"

namespace fpsparty::graphics {
class Descriptor;

namespace detail {

class Descriptor_heap;

struct Descriptor_create_info {
  Descriptor_heap *heap;
  rc::Strong<Image const> image;
  Descriptor_type type;
  u32 handle{};
};

} // namespace detail

class Descriptor {
public:
  ~Descriptor();

  u32 get_handle() const noexcept { return _handle; }

  rc::Strong<Image const> const &get_image() const noexcept { return _image; }

private:
  friend class rc::Factory<Descriptor>;

  explicit Descriptor(detail::Descriptor_create_info info) noexcept;

  detail::Descriptor_heap *_heap{};
  rc::Strong<Image const> _image{};
  Descriptor_type _type;
  u32 _handle{};
};
} // namespace fpsparty::graphics

#endif
