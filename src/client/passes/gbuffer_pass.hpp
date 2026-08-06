#ifndef FPSPARTY_CLIENT_PASSES_GBUFFER_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_GBUFFER_PASS_HPP

#include "client/block_texture_registry.hpp"
#include "client/client.hpp"
#include "client/grid_mesh.hpp"
#include "client/local_player.hpp"
#include "graphics/buffer.hpp"
#include "graphics/pipeline.hpp"
#include "math/mat.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <cstddef>
#include <optional>

namespace fpsparty::client::passes {

struct Gbuffer_pass_inputs {
  render_graph::Symbolic_image albedo_render_target;
  // Normal (oct-encoded) + linear depth + isotropic depth gradient,
  // fused into one target -- see gbuffer.glsl.
  render_graph::Symbolic_image depth_normal_render_target;
  render_graph::Symbolic_image motion_vector_render_target;
  math::ivec2 framebuffer_size;
  std::size_t scene_uniform_offset;
  Client const *client;
  Local_player *local_player;
  // Host-written only (map()) -- never declared, no symbol.
  rc::Strong<graphics::Buffer> scene_uniform_buffer;
  float animation_time;
  Grid_mesh *grid_mesh;
  Block_texture_registry *block_texture_registry;
  rc::Strong<graphics::Buffer> cube_vertex_buffer;
  rc::Strong<graphics::Buffer> cube_index_buffer;
  // Hardware depth test/write only -- nothing samples it, so unlike the
  // targets above it's never declared, no symbol (same reasoning as
  // transmittance_lut elsewhere).
  rc::Strong<graphics::Image> depth_attachment;
};

// Renders the G-buffer (albedo/depth-normal/motion-vector) for the grid
// and entity boxes, and writes this frame's camera/sun fields into
// scene_uniform_buffer via map().
class Gbuffer_pass : public render_graph::Node {
public:
  // previous_view_projection_matrix: last frame's Gbuffer_pass::
  // get_view_projection_matrix() (empty on the first frame with a
  // camera) -- this pass's own cross-frame state, since it's read then
  // overwritten every frame for temporal reprojection and so can't live
  // in a Node rebuilt from scratch each frame; the caller carries it
  // between frames instead.
  Gbuffer_pass(
    rc::Strong<graphics::Pipeline> grid_pipeline,
    rc::Strong<graphics::Pipeline> mesh_pipeline,
    std::size_t cube_index_count,
    float z_near,
    std::optional<math::mat4> previous_view_projection_matrix,
    Gbuffer_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

  // Valid after execute() has run; pass into next frame's constructor.
  std::optional<math::mat4> const &get_view_projection_matrix() const noexcept {
    return _previous_view_projection_matrix;
  }

private:
  rc::Strong<graphics::Pipeline> _grid_pipeline;
  rc::Strong<graphics::Pipeline> _mesh_pipeline;
  std::size_t _cube_index_count;
  float _z_near;
  std::optional<math::mat4> _previous_view_projection_matrix;
  Gbuffer_pass_inputs _inputs;
  render_graph::Resource_handle _albedo_handle{};
  render_graph::Resource_handle _depth_normal_handle{};
  render_graph::Resource_handle _motion_vector_handle{};
};

} // namespace fpsparty::client::passes

#endif
