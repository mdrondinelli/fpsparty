#ifndef FPSPARTY_CLIENT_VERTEX_BUFFER_HPP
#define FPSPARTY_CLIENT_VERTEX_BUFFER_HPP

#include "graphics/graphics_buffer.hpp"
#include "rc.hpp"

namespace fpsparty::graphics {
class Vertex_buffer : public rc::Object<Vertex_buffer>, public Graphics_buffer {
public:
  explicit Vertex_buffer(std::size_t size);
};
} // namespace fpsparty::graphics

#endif
