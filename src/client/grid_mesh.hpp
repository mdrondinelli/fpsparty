#ifndef FPSPARTY_CLIENT_GRID_MESH_HPP
#define FPSPARTY_CLIENT_GRID_MESH_HPP

#include "client/sign.hpp"
#include "game/core/grid.hpp"
#include "graphics/graphics.hpp"
#include "graphics/work_done_callback.hpp"
#include "graphics/work_recorder.hpp"
#include "rc.hpp"
#include <array>
#include <cstdint>

namespace fpsparty::client {
struct Grid_mesh_create_info {
  graphics::Graphics *graphics;
  game::Grid const *grid;
};

class Grid_mesh : graphics::Work_done_callback {
public:
  explicit Grid_mesh(Grid_mesh_create_info const &info);

  ~Grid_mesh();

  void record_draws(
    graphics::Work_recorder &recorder,
    game::Axis normal_axis,
    client::Sign normal_sign);

  bool is_uploaded() const noexcept;

  rc::Strong<graphics::Buffer> const &get_vertex_buffer() const noexcept;

  rc::Strong<graphics::Buffer> const &get_index_buffer() const noexcept;

private:
  struct Indirect_draw_info {
    std::uint64_t offset;
    std::uint32_t draw_count;
  };

  void on_work_done(graphics::Work const &work) override;

  rc::Strong<graphics::Buffer> _vertex_buffer{};
  rc::Strong<graphics::Buffer> _index_buffer{};
  rc::Strong<graphics::Buffer> _draw_buffer{};
  rc::Strong<graphics::Work> _upload_work{};
  std::array<std::array<Indirect_draw_info, 2>, 3> _draw_infos;
};
} // namespace fpsparty::client

#endif
