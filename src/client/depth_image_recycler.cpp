#include "depth_image_recycler.hpp"

namespace fpsparty::client {
Depth_image_factory::Depth_image_factory(graphics::Graphics *graphics) noexcept
    : _graphics{graphics} {}

rc::Strong<graphics::Image> Depth_image_factory::operator()(
    const graphics::recycler_predicates::Image_extent &predicate) const {
  return _graphics->create_image({
      .dimensionality = 2,
      .format = graphics::Image_format::d32_sfloat,
      .extent = predicate.get_extent(),
      .mip_level_count = 1,
      .array_layer_count = 1,
      .usage = graphics::Image_usage_flag_bits::depth_attachment,
  });
}
} // namespace fpsparty::client
