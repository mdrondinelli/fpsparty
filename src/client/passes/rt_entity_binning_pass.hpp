#ifndef FPSPARTY_CLIENT_PASSES_RT_ENTITY_BINNING_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_RT_ENTITY_BINNING_PASS_HPP

#include "client/client.hpp"
#include "client/grid_mesh.hpp"
#include "graphics/buffer.hpp"
#include "graphics/compute_pipeline.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"

namespace fpsparty::client::passes {

struct Rt_entity_binning_pass_inputs {
  Client const *client;
  Grid_mesh *grid_mesh;
  // This frame's _rt_entity_buffers[frame]/_rt_entity_binning_buffers
  // [frame] -- resizing them to fit this frame's entity count
  // (ensure_rt_entity_capacity) stays Application's job, out of the
  // graph's scope, same as render-target creation.
  rc::Strong<graphics::Buffer> entity_buffer;
  rc::Strong<graphics::Buffer> binning_buffer;
};

class Rt_entity_binning_pass : public render_graph::Node {
public:
  explicit Rt_entity_binning_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline);

  // Returns false if there's nothing to bin this frame (no session, or
  // grid mesh not uploaded yet) -- caller should skip add_pass then.
  bool update(Rt_entity_binning_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Rt_entity_binning_pass_inputs _inputs{};
  render_graph::Resource_handle _binning_buffer_handle{};
};

} // namespace fpsparty::client::passes

#endif
