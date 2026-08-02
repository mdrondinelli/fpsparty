#ifndef FPSPARTY_RENDER_GRAPH_RESOURCE_HANDLE_HPP
#define FPSPARTY_RENDER_GRAPH_RESOURCE_HANDLE_HPP

#include "int.hpp"

namespace fpsparty::render_graph {

// Opaque, cheap-to-copy result of Builder::read/write -- resolved back to
// the descriptor declared for it via Resources::get. Only valid for the
// declare/execute pair of the Node call that produced it.
struct Resource_handle {
  u32 index{};

  friend bool
  operator==(Resource_handle lhs, Resource_handle rhs) noexcept = default;
};

} // namespace fpsparty::render_graph

#endif
