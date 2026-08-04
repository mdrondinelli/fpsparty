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
  render_graph::Symbolic_image normal_render_target;
  render_graph::Symbolic_image motion_vector_render_target;
  render_graph::Symbolic_image depth_gradient_render_target;
  render_graph::Symbolic_image depth_render_target;
  math::ivec2 framebuffer_size;
  std::size_t scene_uniform_offset;
  Client const *client;
  Local_player *local_player;
  // Host-write-only from this pass's perspective (map()'d directly, no
  // GPU write here) -- unlike Sky_irradiance_pass's GPU write into this
  // same buffer's disjoint Sky_irradiance sub-struct, so this pass never
  // needs a symbol/barrier for it.
  rc::Strong<graphics::Buffer> scene_uniform_buffer;
  float animation_time;
  Grid_mesh *grid_mesh;
  Block_texture_registry *block_texture_registry;
  rc::Strong<graphics::Buffer> cube_vertex_buffer;
  rc::Strong<graphics::Buffer> cube_index_buffer;
};

// Renders the G-buffer (albedo/normal/motion-vector/depth-gradient/depth)
// for the grid and entity boxes, and writes this frame's Scene camera/sun
// fields into scene_uniform_buffer (CPU-side, via map() -- unrelated to
// the GPU-side write sky_irradiance_pass does into the same buffer's
// Sky_irradiance sub-struct, on disjoint bytes).
class Gbuffer_pass : public render_graph::Node {
public:
  Gbuffer_pass(
    rc::Strong<graphics::Pipeline> grid_pipeline,
    rc::Strong<graphics::Pipeline> mesh_pipeline,
    std::size_t cube_index_count,
    float z_near);

  void update(Gbuffer_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Pipeline> _grid_pipeline;
  rc::Strong<graphics::Pipeline> _mesh_pipeline;
  std::size_t _cube_index_count;
  float _z_near;
  Gbuffer_pass_inputs _inputs{};
  std::optional<math::mat4> _previous_view_projection_matrix{};
  render_graph::Resource_handle _albedo_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _motion_vector_handle{};
  render_graph::Resource_handle _depth_gradient_handle{};
  render_graph::Resource_handle _depth_handle{};
};

} // namespace fpsparty::client::passes

#endif
