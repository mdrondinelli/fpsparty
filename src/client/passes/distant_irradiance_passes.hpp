#ifndef FPSPARTY_CLIENT_PASSES_DISTANT_IRRADIANCE_PASSES_HPP
#define FPSPARTY_CLIENT_PASSES_DISTANT_IRRADIANCE_PASSES_HPP

#include "graphics/buffer.hpp"
#include "graphics/compute_pipeline.hpp"
#include "graphics/image.hpp"
#include "int.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <cstddef>

namespace fpsparty::client::passes {

// Zeroes both sample buffers' atomic counts, so light picking can append
// into them. Its own pass rather than part of that one so that the graph
// places the barrier between the clear and the appends -- passes never
// emit barriers themselves, see render_graph::Node.
struct Distant_irradiance_sample_clear_pass_inputs {
  render_graph::Symbolic_buffer sun_sample_buffer;
  render_graph::Symbolic_buffer sky_sample_buffer;
};

class Distant_irradiance_sample_clear_pass : public render_graph::Node {
public:
  explicit Distant_irradiance_sample_clear_pass(
    Distant_irradiance_sample_clear_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  Distant_irradiance_sample_clear_pass_inputs _inputs;
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
};

// Pass 1: light picking. Chooses sun or sky per pixel with probability
// proportional to each one's analytically integrated unshadowed
// contribution, and appends the pixel to that light's sample buffer.
// The trace passes divide by the selection probability with no MIS
// weight, which is unbiased only because the two emitters are disjoint --
// the sky view LUT carries in-scattered light, never the solar disc.
struct Distant_irradiance_light_pick_pass_inputs {
  // Hardware depth attachment (reverse-Z, sampled directly -- see
  // gbuffer.glsl) and oct-encoded normal (r16g16_sfloat). No gradient
  // needed here.
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  // Never written within any frame's graph (created once at startup) --
  // no symbol, held directly.
  rc::Strong<graphics::Image const> transmittance_lut;
  render_graph::Symbolic_image sky_view_lut;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  render_graph::Symbolic_buffer sun_sample_buffer;
  render_graph::Symbolic_buffer sky_sample_buffer;
  math::ivec2 framebuffer_size;
  u32 frame_number;
};

class Distant_irradiance_light_pick_pass : public render_graph::Node {
public:
  Distant_irradiance_light_pick_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Distant_irradiance_light_pick_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_light_pick_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
};

// Turns the light-pick pass's atomic sample counts into indirect dispatch
// args for the trace passes below, in place in the same two buffers.
struct Distant_irradiance_indirect_args_pass_inputs {
  render_graph::Symbolic_buffer sun_sample_buffer;
  render_graph::Symbolic_buffer sky_sample_buffer;
};

class Distant_irradiance_indirect_args_pass : public render_graph::Node {
public:
  Distant_irradiance_indirect_args_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Distant_irradiance_indirect_args_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_indirect_args_pass_inputs _inputs;
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
};

// Shared by both trace passes below. rt_entity_buffer/rt_block_grid_
// buffer/transmittance_lut are unsymbolized: never written within any
// frame's graph. Trace only produces this frame's raw, unblended
// estimate now -- no history/reprojection here, no previous_*/motion_
// vector fields -- see Distant_irradiance_temporal_pass_inputs below
// for where temporal accumulation happens. No gradient needed here
// either -- only the a-trous filter uses it.
struct Distant_irradiance_trace_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  render_graph::Symbolic_image raw_distant_irradiance_render_target;
  rc::Strong<graphics::Image const> transmittance_lut;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  render_graph::Symbolic_buffer sample_buffer;
  rc::Strong<graphics::Buffer> rt_block_grid_buffer;
  rc::Strong<graphics::Buffer> rt_entity_buffer;
  render_graph::Symbolic_buffer rt_entity_binning_buffer;
  std::size_t rt_entity_binning_grid_offset;
  std::size_t rt_entity_binning_nodes_offset;
  render_graph::Symbolic_image raw_distant_irradiance_luminance_render_target;
};

class Distant_irradiance_trace_sun_pass : public render_graph::Node {
public:
  Distant_irradiance_trace_sun_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Distant_irradiance_trace_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_trace_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _distant_irradiance_handle{};
  render_graph::Resource_handle _luminance_handle{};
};

class Distant_irradiance_trace_sky_pass : public render_graph::Node {
public:
  Distant_irradiance_trace_sky_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Distant_irradiance_trace_pass_inputs inputs,
    render_graph::Symbolic_image sky_view_lut);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_trace_pass_inputs _inputs;
  render_graph::Symbolic_image _sky_view_lut;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _distant_irradiance_handle{};
  render_graph::Resource_handle _luminance_handle{};
};

// Reprojects and temporally accumulates the raw trace output against last
// frame's accumulated history -- see distant_irradiance_temporal.comp.
// previous_* fields are unsymbolized: last frame's already-retired data,
// no this-frame barrier applies (same convention the trace passes used to
// use for these before this pass existed). No gradient needed -- the
// history-reject check only compares depth and normal.
struct Distant_irradiance_temporal_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  rc::Strong<graphics::Image const> previous_depth_render_target;
  rc::Strong<graphics::Image const> previous_normal_render_target;
  // Needed to reconstruct linear depth from the reverse-Z hardware depth
  // values above -- see gbuffer.glsl.
  float z_near;
  render_graph::Symbolic_image motion_vector_render_target;
  render_graph::Symbolic_image raw_distant_irradiance_render_target;
  render_graph::Symbolic_image raw_distant_irradiance_luminance_render_target;
  rc::Strong<graphics::Image const> previous_distant_irradiance_render_target;
  rc::Strong<graphics::Image const>
    previous_distant_irradiance_luminance_render_target;
  u32 history_valid;
  render_graph::Symbolic_image distant_irradiance_render_target;
  render_graph::Symbolic_image distant_irradiance_luminance_render_target;
  math::ivec2 framebuffer_size;
};

class Distant_irradiance_temporal_pass : public render_graph::Node {
public:
  Distant_irradiance_temporal_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Distant_irradiance_temporal_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_temporal_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _motion_vector_handle{};
  render_graph::Resource_handle _raw_distant_irradiance_handle{};
  render_graph::Resource_handle _raw_luminance_handle{};
  render_graph::Resource_handle _distant_irradiance_handle{};
  render_graph::Resource_handle _luminance_handle{};
};

struct Distant_irradiance_variance_pass_inputs {
  // Only depth is needed (sky-mask check) -- normal/gradient aren't used
  // here.
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image distant_irradiance_luminance_render_target;
  render_graph::Symbolic_image distant_irradiance_variance_render_target;
  math::ivec2 framebuffer_size;
};

class Distant_irradiance_variance_pass : public render_graph::Node {
public:
  Distant_irradiance_variance_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Distant_irradiance_variance_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_variance_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _luminance_handle{};
  render_graph::Resource_handle _variance_handle{};
};

// One a-trous iteration; instantiated 5x (steps 1/2/4/8/16) with different
// in/out descriptor pairs each frame. The only pass that needs the
// gradient texture.
struct Distant_irradiance_spatial_filter_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  render_graph::Symbolic_image gradient_render_target;
  // Needed to reconstruct linear depth from the reverse-Z hardware depth
  // values above -- see gbuffer.glsl.
  float z_near;
  render_graph::Symbolic_image color_in;
  render_graph::Symbolic_image variance_in;
  render_graph::Symbolic_image color_out;
  render_graph::Symbolic_image variance_out;
  math::ivec2 framebuffer_size;
};

class Distant_irradiance_spatial_filter_pass : public render_graph::Node {
public:
  Distant_irradiance_spatial_filter_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    u32 step_size,
    Distant_irradiance_spatial_filter_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  u32 _step_size;
  Distant_irradiance_spatial_filter_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _gradient_handle{};
  render_graph::Resource_handle _color_in_handle{};
  render_graph::Resource_handle _variance_in_handle{};
  render_graph::Resource_handle _color_out_handle{};
  render_graph::Resource_handle _variance_out_handle{};
};

} // namespace fpsparty::client::passes

#endif
