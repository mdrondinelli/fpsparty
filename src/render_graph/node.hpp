#ifndef FPSPARTY_RENDER_GRAPH_NODE_HPP
#define FPSPARTY_RENDER_GRAPH_NODE_HPP

#include "graphics/work_recorder.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/resources.hpp"

namespace fpsparty::render_graph {

// A render pass. Owned by the caller; Graph holds a non-owning pointer
// for the duration of one execute() call.
//
// Two rules a Node must keep, neither of which Graph can check:
//
//  - Declare every resource it touches that another Node might also
//    touch. Anything undeclared is invisible to scheduling, so Graph is
//    free to run this Node either side of a conflicting one.
//  - Emit no barriers of its own. Graph decides where the layer
//    boundaries fall and puts exactly one barrier at each; a barrier
//    inside execute() splits whatever layer this Node shares, silently
//    serializing passes that were scheduled to overlap. Work that needs
//    ordering against the rest of the Node belongs in a separate Node,
//    declaring the accesses that imply the barrier.
class Node {
public:
  virtual ~Node() = default;

  // Called once per execute(), before any node's execute(). Declare
  // every resource read/written via builder.read/write and store the
  // returned handles for use in execute().
  virtual void declare(Builder &builder) = 0;

  virtual void
  execute(graphics::Work_recorder &recorder, Resources &resources) = 0;
};

} // namespace fpsparty::render_graph

#endif
