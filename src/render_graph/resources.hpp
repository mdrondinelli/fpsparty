#ifndef FPSPARTY_RENDER_GRAPH_RESOURCES_HPP
#define FPSPARTY_RENDER_GRAPH_RESOURCES_HPP

#include "graphics/descriptor.hpp"
#include "rc.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/resource_handle.hpp"

namespace fpsparty::render_graph {

class Graph;

// Passed to Node::execute -- resolves a Resource_handle from this same
// pass's declare() call back to the descriptor declared for it.
class Resources {
public:
  rc::Strong<graphics::Descriptor> const &get(Resource_handle handle) const;

private:
  friend class Graph;

  explicit Resources(Builder const &builder) noexcept : _builder{&builder} {}

  Builder const *_builder;
};

} // namespace fpsparty::render_graph

#endif
