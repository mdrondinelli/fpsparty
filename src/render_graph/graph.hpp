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

// Runs a sequence of Nodes in the order they were added, inserting a
// barrier before each one covering whatever it reads that an earlier
// node (in this same execute() call) wrote. Execution order is
// declaration order -- this does not topologically sort or reorder
// passes, only computes barriers from the declared accesses.
//
// Nodes declare against symbolic resources, never concrete ones --
// execute() itself takes this frame's complete symbol->resource binding
// for each resource kind, as part of the same call that runs the graph,
// so there's no separate "bound but not yet executed" state and no way
// to run against a partially-bound frame. Cleared after execute()
// returns, ready for the next frame's add_pass calls.
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
