#ifndef FPSPARTY_CLIENT_DEPTH_IMAGE_RECYCLER_HPP
#define FPSPARTY_CLIENT_DEPTH_IMAGE_RECYCLER_HPP

#include "graphics/graphics.hpp"
#include "graphics/image.hpp"
#include "graphics/recycler.hpp"

namespace fpsparty::client {
class Depth_image_factory {
public:
  explicit Depth_image_factory(graphics::Graphics *graphics) noexcept;

  rc::Strong<graphics::Image> operator()(
    const graphics::recycler_predicates::Image_extent &predicate) const;

private:
  graphics::Graphics *_graphics;
};

using Depth_image_recycler = graphics::Recycler<
  graphics::Image,
  graphics::recycler_predicates::Image_extent,
  Depth_image_factory>;
} // namespace fpsparty::client

#endif
