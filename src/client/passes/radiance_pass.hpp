#ifndef FPSPARTY_CLIENT_PASSES_RADIANCE_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_RADIANCE_PASS_HPP

#include "client/client.hpp"
#include "client/grid_mesh.hpp"
#include "client/local_player.hpp"
#include "graphics/compute_pipeline.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"

namespace fpsparty::client::passes {

struct Radiance_pass_inputs {
  Client const *client;
  Local_player *local_player;
  Grid_mesh *grid_mesh;
  // has_camera() writes via a storage-descriptor index derived from the
  // same image resources resolves for _radiance_handle.
  render_graph::Symbolic_image radiance_render_target;
  render_graph::Symbolic_image albedo_render_target;
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image sky_view_lut;
  render_graph::Symbolic_image distant_irradiance_filtered;
  math::ivec2 framebuffer_size;
};

// Lights the G-buffer, or if there's nothing to light (no camera, or the
// grid mesh hasn't finished uploading -- see Gbuffer_pass), just clears
// the target to sky_color instead of dispatching with no valid view/grid.
class Radiance_pass : public render_graph::Node {
public:
  Radiance_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline, Radiance_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  bool has_camera() const;

  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Radiance_pass_inputs _inputs;
  render_graph::Resource_handle _radiance_handle{};
  render_graph::Resource_handle _albedo_handle{};
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _distant_irradiance_filtered_handle{};
};

} // namespace fpsparty::client::passes

#endif
