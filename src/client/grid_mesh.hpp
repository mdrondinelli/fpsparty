#ifndef FPSPARTY_CLIENT_GRID_MESH_HPP
#define FPSPARTY_CLIENT_GRID_MESH_HPP

#include "game/core/grid.hpp"
#include "graphics/graphics.hpp"
#include "graphics/index_buffer.hpp"
#include "graphics/vertex_buffer.hpp"
#include "graphics/work_done_callback.hpp"
#include "graphics/work_recorder.hpp"
#include "rc.hpp"

namespace fpsparty::client {
struct Grid_mesh_create_info {
  graphics::Graphics *graphics;
  const game::Grid *grid;
};

class Grid_mesh : graphics::Work_done_callback {
public:
  explicit Grid_mesh(const Grid_mesh_create_info &info);

  ~Grid_mesh();

  void record_face_drawing_command(
    graphics::Work_recorder &recorder, game::Axis axis
  );

  void record_edge_drawing_command(
    graphics::Work_recorder &recorder, game::Axis axis
  );

  bool is_uploaded() const noexcept;

  const rc::Strong<graphics::Vertex_buffer> &get_vertex_buffer() const noexcept;

  const rc::Strong<graphics::Index_buffer> &get_index_buffer() const noexcept;

private:
  void on_work_done(const graphics::Work &work) override;

  rc::Strong<graphics::Vertex_buffer> _vertex_buffer{};
  rc::Strong<graphics::Index_buffer> _index_buffer{};
  rc::Strong<graphics::Work> _upload_work{};
  std::array<graphics::Indexed_draw_info, 3> _face_draw_infos;
};
} // namespace fpsparty::client

#endif
