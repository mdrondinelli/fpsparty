#ifndef FPSPARTY_CLIENT_TEXTURE_MANAGER_HPP
#define FPSPARTY_CLIENT_TEXTURE_MANAGER_HPP

#include <cstddef>
#include <vector>

#include <graphics/graphics.hpp>

namespace fpsparty::client {

auto constexpr blue_noise_texture_count = std::size_t{64};

enum class Texture {
  placeholder,
  stone,
  dirt,
  conveyor_belt,
  conveyor_side,
  red,
};

struct Texture_manager_create_info {
  graphics::Graphics *graphics;
};

class Texture_manager {
public:
  // blocks until all textures are loaded
  explicit Texture_manager(Texture_manager_create_info const &info);

  rc::Strong<graphics::Image> get(Texture texture) const noexcept;

  rc::Strong<graphics::Image> get_blue_noise(std::size_t index) const noexcept;

private:
  std::vector<rc::Strong<graphics::Image>> _images;
  std::vector<rc::Strong<graphics::Image>> _blue_noise_images;
};

} // namespace fpsparty::client

#endif
