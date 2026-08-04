#ifndef FPSPARTY_CLIENT_PASSES_RADIANCE_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_RADIANCE_PASS_HPP

#include "client/client.hpp"
#include "client/grid_mesh.hpp"
#include "client/local_player.hpp"
#include "graphics/compute_pipeline.hpp"
#include "graphics/descriptor.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"

namespace fpsparty::client::passes {

struct Radiance_pass_inputs {
  Client const *client;
  Local_player *local_player;
  Grid_mesh *grid_mesh;
  render_graph::Symbolic_image radiance_render_target;
  // Used directly, never declared: the write into it (via imageStore in
  // the shader) happens in the same execute() call that writes
  // radiance_render_target itself (through the symbol above), so no
  // other Node ever needs a barrier keyed on this specific descriptor.
  rc::Strong<graphics::Descriptor> radiance_render_target_storage_descriptor;
  render_graph::Symbolic_descriptor albedo_descriptor;
  render_graph::Symbolic_descriptor depth_descriptor;
  render_graph::Symbolic_descriptor sky_view_lut_sampled_descriptor;
  render_graph::Symbolic_descriptor distant_irradiance_filtered_descriptor;
  math::ivec2 framebuffer_size;
};

// Lights the G-buffer, or if there's nothing to light (no camera, or the
// grid mesh hasn't finished uploading -- see Gbuffer_pass), just clears
// the target to sky_color instead of dispatching with no valid view/grid.
class Radiance_pass : public render_graph::Node {
public:
  explicit Radiance_pass(rc::Strong<graphics::Compute_pipeline> pipeline);

  void update(Radiance_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  bool has_camera() const;

  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Radiance_pass_inputs _inputs{};
  render_graph::Resource_handle _radiance_handle{};
  render_graph::Resource_handle _albedo_handle{};
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _distant_irradiance_filtered_handle{};
};

} // namespace fpsparty::client::passes

#endif
