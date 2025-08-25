#ifndef FPSPARTY_GRAPHICS_INDEX_BUFFER_HPP
#define FPSPARTY_GRAPHICS_INDEX_BUFFER_HPP

#include "graphics/buffer.hpp"

namespace fpsparty::graphics {
class Index_buffer : public Buffer {
public:
  explicit Index_buffer(std::size_t size);
};
} // namespace fpsparty::graphics

#endif
