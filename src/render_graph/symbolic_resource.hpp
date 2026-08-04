#ifndef FPSPARTY_RENDER_GRAPH_SYMBOLIC_RESOURCE_HPP
#define FPSPARTY_RENDER_GRAPH_SYMBOLIC_RESOURCE_HPP

#include "int.hpp"
#include <cstddef>
#include <functional>

namespace fpsparty::render_graph {

// Opaque per-logical-resource identity, allocated once by
// Graph::allocate_*_symbol() and handed to every consuming Node's
// constructor/update(). Carries no concrete resource -- declare() only
// ever sees the symbol; Graph::execute resolves it against whatever was
// most recently Graph::provide_*'d for it this frame.
struct Symbolic_image {
  u32 id{};

  friend bool
  operator==(Symbolic_image lhs, Symbolic_image rhs) noexcept = default;
};

struct Symbolic_buffer {
  u32 id{};

  friend bool
  operator==(Symbolic_buffer lhs, Symbolic_buffer rhs) noexcept = default;
};

} // namespace fpsparty::render_graph

namespace std {
template <> struct hash<fpsparty::render_graph::Symbolic_image> {
  std::size_t
  operator()(fpsparty::render_graph::Symbolic_image s) const noexcept {
    return std::hash<fpsparty::u32>{}(s.id);
  }
};
template <> struct hash<fpsparty::render_graph::Symbolic_buffer> {
  std::size_t
  operator()(fpsparty::render_graph::Symbolic_buffer s) const noexcept {
    return std::hash<fpsparty::u32>{}(s.id);
  }
};
} // namespace std

#endif
