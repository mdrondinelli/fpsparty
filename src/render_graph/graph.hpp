#ifndef FPSPARTY_RENDER_GRAPH_GRAPH_HPP
#define FPSPARTY_RENDER_GRAPH_GRAPH_HPP

#include "graphics/buffer.hpp"
#include "graphics/image.hpp"
#include "graphics/work_recorder.hpp"
#include "rc.hpp"
#include "render_graph/access.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <initializer_list>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fpsparty::render_graph {

// Runs Nodes in add_pass() order (declaration order, not topologically
// sorted), inserting one barrier before each node covering whatever it
// reads that an earlier node in the same execute() call wrote.
//
// Nodes declare against symbolic resources. execute() resolves every
// symbol from the images/buffers arguments passed to that same call.
// State clears after execute() returns.
class Graph {
public:
  void add_pass(Node &node);

  Symbolic_image allocate_image_symbol() noexcept;
  Symbolic_buffer allocate_buffer_symbol() noexcept;

  void execute(
    graphics::Work_recorder &recorder,
    std::initializer_list<
      std::pair<Symbolic_image, rc::Strong<graphics::Image>>> images,
    std::initializer_list<
      std::pair<Symbolic_buffer, rc::Strong<graphics::Buffer>>> buffers);

private:
  friend class Resources;

  rc::Strong<graphics::Image> const &
  resolve_image(Symbolic_image symbol) const;

  rc::Strong<graphics::Buffer> const &
  resolve_buffer(Symbolic_buffer symbol) const;

  std::vector<Node *> _nodes{};
  Builder _builder{};
  u32 _next_image_symbol{};
  u32 _next_buffer_symbol{};
  std::unordered_map<Symbolic_image, rc::Strong<graphics::Image>>
    _provided_images{};
  std::unordered_map<Symbolic_buffer, rc::Strong<graphics::Buffer>>
    _provided_buffers{};
  std::unordered_map<rc::Strong<graphics::Image const>, Access>
    _last_image_write{};
  std::unordered_map<rc::Strong<graphics::Buffer const>, Access>
    _last_buffer_write{};
};

} // namespace fpsparty::render_graph

#endif
