#ifndef FPSPARTY_CLIENT_GRID_MESH_HPP
#define FPSPARTY_CLIENT_GRID_MESH_HPP

#include "game/core/grid.hpp"
#include "graphics/staging_buffer.hpp"
#include "graphics/vertex_buffer.hpp"
#include "rc.hpp"
#include <utility>

namespace fpsparty::client {
class Grid_mesh {
public:
  void update(const game::Grid &grid);

  std::pair<rc::Strong<graphics::Vertex_buffer>,
            rc::Strong<graphics::Staging_buffer>>
  get_current_buffers() const;
};
} // namespace fpsparty::client

#endif
