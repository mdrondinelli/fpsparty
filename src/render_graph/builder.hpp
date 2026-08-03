#ifndef FPSPARTY_RENDER_GRAPH_BUILDER_HPP
#define FPSPARTY_RENDER_GRAPH_BUILDER_HPP

#include "graphics/buffer.hpp"
#include "graphics/descriptor.hpp"
#include "graphics/image.hpp"
#include "rc.hpp"
#include "render_graph/access.hpp"
#include "render_graph/resource_handle.hpp"
#include <variant>
#include <vector>

namespace fpsparty::render_graph {

class Graph;
class Resources;

// Passed to Node::declare -- records each resource a pass touches (images
// keyed by identity via Descriptor::get_image, so a sampled-descriptor
// read and either a storage-descriptor write or a raw color/depth-
// attachment write of the same image are recognized as the same
// resource; buffers keyed directly) so Graph::execute can compute the
// barrier needed before the pass runs.
class Builder {
public:
  Resource_handle
  read(rc::Strong<graphics::Descriptor> descriptor, Access access);

  Resource_handle
  write(rc::Strong<graphics::Descriptor> descriptor, Access access);

  Resource_handle read(rc::Strong<graphics::Image const> image, Access access);

  Resource_handle
  write(rc::Strong<graphics::Image const> image, Access access);

  Resource_handle
  read(rc::Strong<graphics::Buffer const> buffer, Access access);

  Resource_handle
  write(rc::Strong<graphics::Buffer const> buffer, Access access);

private:
  friend class Graph;
  friend class Resources;

  using Payload = std::variant<
    rc::Strong<graphics::Descriptor>,
    rc::Strong<graphics::Image const>,
    rc::Strong<graphics::Buffer const>>;

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
