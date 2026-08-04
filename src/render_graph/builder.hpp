#ifndef FPSPARTY_RENDER_GRAPH_BUILDER_HPP
#define FPSPARTY_RENDER_GRAPH_BUILDER_HPP

#include "render_graph/access.hpp"
#include "render_graph/resource_handle.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <variant>
#include <vector>

namespace fpsparty::render_graph {

class Graph;
class Resources;

// Passed to Node::declare -- records each symbolic resource a pass
// touches so Graph::execute can resolve it to a concrete resource and
// compute the barrier needed before the pass runs. declare() never sees
// a concrete resource, only the opaque symbol identifying it.
class Builder {
public:
  Resource_handle read(Symbolic_image image, Access access);

  Resource_handle write(Symbolic_image image, Access access);

  Resource_handle read(Symbolic_buffer buffer, Access access);

  Resource_handle write(Symbolic_buffer buffer, Access access);

private:
  friend class Graph;
  friend class Resources;

  using Payload = std::variant<Symbolic_image, Symbolic_buffer>;

  struct Entry {
    Payload payload;
    Access access;
    bool is_write;
  };

  Resource_handle add(Payload payload, Access access, bool is_write);

  std::vector<Entry> _entries{};
};

} // namespace fpsparty::render_graph

#endif
