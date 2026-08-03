#ifndef FPSPARTY_RENDER_GRAPH_RESOURCES_HPP
#define FPSPARTY_RENDER_GRAPH_RESOURCES_HPP

#include "graphics/buffer.hpp"
#include "graphics/descriptor.hpp"
#include "graphics/image.hpp"
#include "rc.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/resource_handle.hpp"

namespace fpsparty::render_graph {

class Graph;

// Passed to Node::execute -- resolves a Resource_handle from this same
// pass's declare() call back to what it was declared with. Calling the
// accessor that doesn't match how the handle was declared (e.g.
// get_buffer on a handle from builder.write(image, ...)) is a caller bug;
// std::get throws rather than silently returning a wrong value.
class Resources {
public:
  rc::Strong<graphics::Descriptor> const &
  get_descriptor(Resource_handle handle) const;

  rc::Strong<graphics::Image const> const &
  get_image(Resource_handle handle) const;

  rc::Strong<graphics::Buffer const> const &
  get_buffer(Resource_handle handle) const;

private:
  friend class Graph;

  explicit Resources(Builder const &builder) noexcept : _builder{&builder} {}

  Builder const *_builder;
};

} // namespace fpsparty::render_graph

#endif
