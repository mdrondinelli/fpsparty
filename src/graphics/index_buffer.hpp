#ifndef FPSPARTY_CLIENT_INDEX_BUFFER_HPP
#define FPSPARTY_CLIENT_INDEX_BUFFER_HPP

#include "graphics/graphics_buffer.hpp"
#include "rc.hpp"

namespace fpsparty::graphics {
class Index_buffer : public rc::Object<Index_buffer>, public Graphics_buffer {
public:
  explicit Index_buffer(std::size_t size);
};
} // namespace fpsparty::graphics

#endif
