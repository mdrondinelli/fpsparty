#ifndef FPSPARTY_CLIENT_TEXTURE_MANAGER_HPP
#define FPSPARTY_CLIENT_TEXTURE_MANAGER_HPP

#include <graphics/graphics.hpp>

namespace fpsparty::client {

enum class Texture {
  placeholder,
  stone,
  dirt,
  conveyor_belt,
  conveyor_side,
};

struct Texture_manager_create_info {
  graphics::Graphics *graphics;
};

class Texture_manager {
public:
  // blocks until all textures are loaded
  explicit Texture_manager(Texture_manager_create_info const &info);

  rc::Strong<graphics::Image> get(Texture texture) const noexcept;

private:
  std::vector<rc::Strong<graphics::Image>> _images;
};

} // namespace fpsparty::client

#endif
