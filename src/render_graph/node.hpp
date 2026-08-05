#ifndef FPSPARTY_RENDER_GRAPH_NODE_HPP
#define FPSPARTY_RENDER_GRAPH_NODE_HPP

#include "graphics/work_recorder.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/resources.hpp"

namespace fpsparty::render_graph {

// A render pass. Owned by the caller; Graph holds a non-owning pointer
// for the duration of one execute() call.
class Node {
public:
  virtual ~Node() = default;

  // Called once per execute(), before this node's execute(). Declare
  // every resource read/written via builder.read/write and store the
  // returned handles for use in execute().
  virtual void declare(Builder &builder) = 0;

  virtual void
  execute(graphics::Work_recorder &recorder, Resources &resources) = 0;
};

} // namespace fpsparty::render_graph

#endif
