#ifndef FPSPARTY_CLIENT_GRID_VERTEX_HPP
#define FPSPARTY_CLIENT_GRID_VERTEX_HPP

#include <math/vec.hpp>

namespace fpsparty::client {
struct Grid_vertex {
  math::vec3 position;
  math::vec2 texcoord;
  std::uint32_t texture_index;
};

static_assert(sizeof(Grid_vertex) == 24);
static_assert(offsetof(Grid_vertex, position) == 0);
static_assert(offsetof(Grid_vertex, texcoord) == 12);
static_assert(offsetof(Grid_vertex, texture_index) == 20);
} // namespace fpsparty::client

#endif
