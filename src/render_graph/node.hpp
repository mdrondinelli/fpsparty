#ifndef FPSPARTY_RENDER_GRAPH_NODE_HPP
#define FPSPARTY_RENDER_GRAPH_NODE_HPP

#include "graphics/work_recorder.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/resources.hpp"

namespace fpsparty::render_graph {

// A render pass. Owned by the caller (e.g. as an Application member or a
// local it keeps alive across the call to Graph::execute) -- Graph only
// holds a non-owning pointer to it for the duration of one execute() call.
class Node {
public:
  virtual ~Node() = default;

  // Called once per Graph::execute(), before execute() -- declare every
  // resource this pass reads/writes via builder.read/write, and stash the
  // returned handles (as member variables) for use in execute() below.
  virtual void declare(Builder &builder) = 0;

  virtual void
  execute(graphics::Work_recorder &recorder, Resources &resources) = 0;
};

} // namespace fpsparty::render_graph

#endif
