#ifndef FPSPARTY_CLIENT_GRID_VERTEX_HPP
#define FPSPARTY_CLIENT_GRID_VERTEX_HPP

#include <math/vec.hpp>

namespace fpsparty::client {
struct Grid_vertex {
  math::vec3 position;
  std::uint32_t texture_index;
};

static_assert(sizeof(Grid_vertex) == 16);
static_assert(offsetof(Grid_vertex, position) == 0);
static_assert(offsetof(Grid_vertex, texture_index) == 12);
} // namespace fpsparty::client

#endif
