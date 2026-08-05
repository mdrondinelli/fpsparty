#ifndef FPSPARTY_RENDER_GRAPH_RESOURCES_HPP
#define FPSPARTY_RENDER_GRAPH_RESOURCES_HPP

#include "graphics/buffer.hpp"
#include "graphics/image.hpp"
#include "rc.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/resource_handle.hpp"

namespace fpsparty::render_graph {

class Graph;

// Passed to Node::execute. Resolves a Resource_handle from this same
// declare() call to its concrete resource. Calling the accessor for the
// wrong kind (e.g. get_buffer on a handle from builder.write(image,
// ...)) throws.
class Resources {
public:
  rc::Strong<graphics::Image> const & get_image(Resource_handle handle) const;

  rc::Strong<graphics::Buffer> const &
  get_buffer(Resource_handle handle) const;

private:
  friend class Graph;

  explicit Resources(Builder const &builder, Graph const &graph) noexcept
      : _builder{&builder}, _graph{&graph} {}

  Builder const *_builder;
  Graph const *_graph;
};

} // namespace fpsparty::render_graph

#endif
