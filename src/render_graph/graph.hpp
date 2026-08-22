#ifndef FPSPARTY_RENDER_GRAPH_GRAPH_HPP
#define FPSPARTY_RENDER_GRAPH_GRAPH_HPP

#include "graphics/buffer.hpp"
#include "graphics/image.hpp"
#include "graphics/work_recorder.hpp"
#include "rc.hpp"
#include "render_graph/access.hpp"
#include "render_graph/builder.hpp"
#include "render_graph/node.hpp"
#include "render_graph/schedule.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <initializer_list>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace fpsparty::render_graph {

// Identifies a pass within one execute() batch, for set_disjoint.
struct Pass_handle {
  u32 index{};

  friend bool
  operator==(Pass_handle lhs, Pass_handle rhs) noexcept = default;
};

// Schedules Nodes rather than running them in add_pass() order. execute()
// declares every node, derives RAW/WAW/WAR hazards from those
// declarations, assigns each node to the earliest layer its dependencies
// allow, and records layer by layer with one barrier per boundary.
//
// add_pass() order decides only which side of a conflicting access pair
// is the producer. It does not otherwise constrain execution, so a Node's
// execute() must not have record-time effects another Node depends on.
// Two exceptions the graph cannot see: a Node emitting its own barrier or
// layout transition splits any layer it shares.
//
// Nodes declare against symbolic resources. execute() resolves every
// symbol from the images/buffers arguments passed to that same call.
// State clears after execute() returns.
class Graph {
public:
  Pass_handle add_pass(Node &node);

  // Promises the two passes touch disjoint parts of the resource, so
  // hazards between them need no ordering. Unchecked -- a wrong promise
  // is a race.
  void set_disjoint(Pass_handle a, Pass_handle b, Symbolic_image resource);
  void set_disjoint(Pass_handle a, Pass_handle b, Symbolic_buffer resource);

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

  using Resource_payload = std::variant<Symbolic_image, Symbolic_buffer>;

  struct Disjoint_declaration {
    u32 pass_a{};
    u32 pass_b{};
    Resource_payload resource{};
  };

  rc::Strong<graphics::Image> const &
  resolve_image(Symbolic_image symbol) const;

  rc::Strong<graphics::Buffer> const &
  resolve_buffer(Symbolic_buffer symbol) const;

  // Dense id for the concrete resource a symbol resolves to. Two symbols
  // aliasing one resource share an id.
  u32 resource_id(Resource_payload const &payload);

  std::vector<Node *> _nodes{};
  // One per node, kept across frames so the entry vectors keep capacity.
  std::vector<Builder> _builders{};
  std::vector<Disjoint_declaration> _disjoint{};
  u32 _next_image_symbol{};
  u32 _next_buffer_symbol{};
  std::unordered_map<Symbolic_image, rc::Strong<graphics::Image>>
    _provided_images{};
  std::unordered_map<Symbolic_buffer, rc::Strong<graphics::Buffer>>
    _provided_buffers{};
  std::unordered_map<rc::Strong<graphics::Image const>, u32> _image_ids{};
  std::unordered_map<rc::Strong<graphics::Buffer const>, u32> _buffer_ids{};
  u32 _next_resource_id{};
};

} // namespace fpsparty::render_graph

#endif
