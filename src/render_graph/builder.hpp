#ifndef FPSPARTY_RENDER_GRAPH_BUILDER_HPP
#define FPSPARTY_RENDER_GRAPH_BUILDER_HPP

#include "graphics/descriptor.hpp"
#include "rc.hpp"
#include "render_graph/access.hpp"
#include "render_graph/resource_handle.hpp"
#include <vector>

namespace fpsparty::render_graph {

class Graph;
class Resources;

// Passed to Node::declare -- records each resource a pass touches (keyed
// by the underlying image's identity, via Descriptor::get_image, so a
// sampled-descriptor read and a storage-descriptor write of the same
// image are recognized as the same resource) so Graph::execute can
// compute the barrier needed before the pass runs.
class Builder {
public:
  Resource_handle
  read(rc::Strong<graphics::Descriptor> descriptor, Access access);

  Resource_handle
  write(rc::Strong<graphics::Descriptor> descriptor, Access access);

private:
  friend class Graph;
  friend class Resources;

  struct Entry {
    rc::Strong<graphics::Descriptor> descriptor;
    Access access;
    bool is_write;
  };

  std::vector<Entry> _entries{};
};

} // namespace fpsparty::render_graph

#endif
