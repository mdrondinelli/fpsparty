#ifndef FPSPARTY_RENDER_GRAPH_GRAPH_HPP
#define FPSPARTY_RENDER_GRAPH_GRAPH_HPP

#include "graphics/buffer.hpp"
#include "graphics/image.hpp"
#include "graphics/work_recorder.hpp"
#include "rc.hpp"
#include "render_graph/access.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/node.hpp"
#include <unordered_map>
#include <vector>

namespace fpsparty::render_graph {

// Runs a sequence of Nodes in the order they were added, inserting a
// barrier before each one covering whatever it reads that an earlier
// node (in this same execute() call) wrote. Execution order is
// declaration order -- this does not topologically sort or reorder
// passes, only computes barriers from the declared accesses. Cleared
// after each execute() call, ready for the next frame's add_pass calls.
class Graph {
public:
  void add_pass(Node &node);

  void execute(graphics::Work_recorder &recorder);

private:
  std::vector<Node *> _nodes{};
  Builder _builder{};
  std::unordered_map<rc::Strong<graphics::Image const>, Access>
    _last_image_write{};
  std::unordered_map<rc::Strong<graphics::Buffer const>, Access>
    _last_buffer_write{};
};

} // namespace fpsparty::render_graph

#endif
