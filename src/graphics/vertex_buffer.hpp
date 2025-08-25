#ifndef FPSPARTY_GRAPHICS_VERTEX_BUFFER_HPP
#define FPSPARTY_GRAPHICS_VERTEX_BUFFER_HPP

#include "graphics/buffer.hpp"

namespace fpsparty::graphics {
class Vertex_buffer : public Buffer {
public:
  explicit Vertex_buffer(std::size_t size);
};
} // namespace fpsparty::graphics

#endif
