#ifndef FPSPARTY_CLIENT_PASSES_DISTANT_IRRADIANCE_PASSES_HPP
#define FPSPARTY_CLIENT_PASSES_DISTANT_IRRADIANCE_PASSES_HPP

#include "graphics/buffer.hpp"
#include "graphics/compute_pipeline.hpp"
#include "graphics/descriptor.hpp"
#include "int.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <cstddef>

namespace fpsparty::client::passes {

// Pass 1: RIS-picks sun vs sky per pixel, appending each pixel to
// whichever of the two sample buffers it picked (their atomic counts are
// zeroed here too, immediately before the dispatch that appends to them --
// an internal detail, not a cross-pass dependency, so not declared).
struct Distant_irradiance_pass1_inputs {
  render_graph::Symbolic_descriptor normal_descriptor;
  render_graph::Symbolic_descriptor depth_descriptor;
  // Never written within any frame's graph (created once at startup) --
  // no symbol, held directly.
  rc::Strong<graphics::Descriptor> transmittance_lut_sampled_descriptor;
  render_graph::Symbolic_descriptor sky_view_lut_sampled_descriptor;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  render_graph::Symbolic_buffer sun_sample_buffer;
  render_graph::Symbolic_buffer sky_sample_buffer;
  math::ivec2 framebuffer_size;
  u32 frame_number;
};

class Distant_irradiance_pass1 : public render_graph::Node {
public:
  explicit Distant_irradiance_pass1(
    rc::Strong<graphics::Compute_pipeline> pipeline);

  void update(Distant_irradiance_pass1_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_pass1_inputs _inputs{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
};

// Turns pass 1's atomic sample counts into indirect dispatch args for the
// trace passes below, in place in the same two buffers.
struct Distant_irradiance_indirect_args_pass_inputs {
  render_graph::Symbolic_buffer sun_sample_buffer;
  render_graph::Symbolic_buffer sky_sample_buffer;
};

class Distant_irradiance_indirect_args_pass : public render_graph::Node {
public:
  explicit Distant_irradiance_indirect_args_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline);

  void update(Distant_irradiance_indirect_args_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_indirect_args_pass_inputs _inputs{};
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
};

// Shared by both trace passes below. previous_*/rt_entity_buffer/
// rt_block_grid_buffer/transmittance_lut_sampled_descriptor stay plain:
// the previous_* fields reference last frame's already-retired work (the
// cross-frame timeline-semaphore wait protects those, not this frame's
// Graph barriers), and the rest are never written within any frame's
// graph.
struct Distant_irradiance_trace_pass_inputs {
  render_graph::Symbolic_descriptor normal_descriptor;
  render_graph::Symbolic_descriptor depth_descriptor;
  render_graph::Symbolic_descriptor distant_irradiance_storage_descriptor;
  rc::Strong<graphics::Descriptor> transmittance_lut_sampled_descriptor;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  render_graph::Symbolic_buffer sample_buffer;
  rc::Strong<graphics::Buffer> rt_block_grid_buffer;
  rc::Strong<graphics::Buffer> rt_entity_buffer;
  render_graph::Symbolic_buffer rt_entity_binning_buffer;
  std::size_t rt_entity_binning_grid_offset;
  std::size_t rt_entity_binning_nodes_offset;
  rc::Strong<graphics::Descriptor> previous_depth_descriptor;
  rc::Strong<graphics::Descriptor> previous_distant_irradiance_descriptor;
  render_graph::Symbolic_descriptor motion_vector_descriptor;
  rc::Strong<graphics::Descriptor> previous_normal_descriptor;
  u32 history_valid;
  render_graph::Symbolic_descriptor luminance_storage_descriptor;
  rc::Strong<graphics::Descriptor> previous_luminance_descriptor;
};

class Distant_irradiance_trace_sun_pass : public render_graph::Node {
public:
  explicit Distant_irradiance_trace_sun_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline);

  void update(Distant_irradiance_trace_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_trace_pass_inputs _inputs{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _motion_vector_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _distant_irradiance_handle{};
  render_graph::Resource_handle _luminance_handle{};
};

class Distant_irradiance_trace_sky_pass : public render_graph::Node {
public:
  explicit Distant_irradiance_trace_sky_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline);

  void update(
    Distant_irradiance_trace_pass_inputs inputs,
    render_graph::Symbolic_descriptor sky_view_lut_sampled_descriptor);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_trace_pass_inputs _inputs{};
  render_graph::Symbolic_descriptor _sky_view_lut_sampled_descriptor{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _motion_vector_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _distant_irradiance_handle{};
  render_graph::Resource_handle _luminance_handle{};
};

struct Distant_irradiance_variance_pass_inputs {
  render_graph::Symbolic_descriptor depth_descriptor;
  render_graph::Symbolic_descriptor luminance_descriptor;
  render_graph::Symbolic_descriptor variance_storage_descriptor;
  math::ivec2 framebuffer_size;
};

class Distant_irradiance_variance_pass : public render_graph::Node {
public:
  explicit Distant_irradiance_variance_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline);

  void update(Distant_irradiance_variance_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_variance_pass_inputs _inputs{};
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _luminance_handle{};
  render_graph::Resource_handle _variance_handle{};
};

// One a-trous iteration; instantiated 5x (steps 1/2/4/8/16) with different
// in/out descriptor pairs each frame.
struct Distant_irradiance_spatial_filter_pass_inputs {
  render_graph::Symbolic_descriptor depth_descriptor;
  render_graph::Symbolic_descriptor normal_descriptor;
  render_graph::Symbolic_descriptor depth_gradient_descriptor;
  render_graph::Symbolic_descriptor color_in_descriptor;
  render_graph::Symbolic_descriptor variance_in_descriptor;
  render_graph::Symbolic_descriptor color_out_storage_descriptor;
  render_graph::Symbolic_descriptor variance_out_storage_descriptor;
  math::ivec2 framebuffer_size;
};

class Distant_irradiance_spatial_filter_pass : public render_graph::Node {
public:
  Distant_irradiance_spatial_filter_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline, u32 step_size);

  void update(Distant_irradiance_spatial_filter_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  u32 _step_size;
  Distant_irradiance_spatial_filter_pass_inputs _inputs{};
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _depth_gradient_handle{};
  render_graph::Resource_handle _color_in_handle{};
  render_graph::Resource_handle _variance_in_handle{};
  render_graph::Resource_handle _color_out_handle{};
  render_graph::Resource_handle _variance_out_handle{};
};

} // namespace fpsparty::client::passes

#endif
