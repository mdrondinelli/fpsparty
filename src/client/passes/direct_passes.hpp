#ifndef FPSPARTY_CLIENT_PASSES_DIRECT_PASSES_HPP
#define FPSPARTY_CLIENT_PASSES_DIRECT_PASSES_HPP

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
struct Direct_sample_clear_pass_inputs {
  render_graph::Symbolic_buffer sun_sample_buffer;
  render_graph::Symbolic_buffer sky_sample_buffer;
  render_graph::Symbolic_buffer brdf_sample_buffer;
};

class Direct_sample_clear_pass : public render_graph::Node {
public:
  explicit Direct_sample_clear_pass(
    Direct_sample_clear_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  Direct_sample_clear_pass_inputs _inputs;
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
  render_graph::Resource_handle _brdf_sample_handle{};
};

// Light picking, and the sample lists for both MIS techniques.
//
// Technique A is the picked ray: sun or sky with probability proportional
// to each one's analytically integrated unshadowed contribution, appended
// to that light's sample buffer. Technique B is one cosine-weighted BRDF
// ray per valid pixel, appended unconditionally to a third buffer.
//
// The picked probability is stored in each sample so the traces can form
// the MIS denominators without re-deriving it -- see
// direct_common.glsl.
struct Direct_light_pick_pass_inputs {
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
  render_graph::Symbolic_buffer brdf_sample_buffer;
  math::ivec2 framebuffer_size;
  u32 frame_number;
};

class Direct_light_pick_pass : public render_graph::Node {
public:
  Direct_light_pick_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_light_pick_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_light_pick_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
  render_graph::Resource_handle _brdf_sample_handle{};
};

// Turns the light-pick pass's atomic sample counts into indirect dispatch
// args for the trace passes below, in place in the same three buffers.
struct Direct_indirect_args_pass_inputs {
  render_graph::Symbolic_buffer sun_sample_buffer;
  render_graph::Symbolic_buffer sky_sample_buffer;
  render_graph::Symbolic_buffer brdf_sample_buffer;
};

class Direct_indirect_args_pass : public render_graph::Node {
public:
  Direct_indirect_args_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_indirect_args_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_indirect_args_pass_inputs _inputs;
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
  render_graph::Resource_handle _brdf_sample_handle{};
};

// Shared by all three trace passes below. rt_entity_buffer/rt_block_grid_
// buffer/transmittance_lut are unsymbolized: never written within any
// frame's graph. A trace produces one frame's raw, unblended estimate;
// Direct_temporal_pass sums the techniques and accumulates.
// No gradient is needed here -- only the a-trous filter uses it.
struct Direct_trace_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  render_graph::Symbolic_image raw_direct_irradiance_render_target;
  rc::Strong<graphics::Image const> transmittance_lut;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  render_graph::Symbolic_buffer sample_buffer;
  rc::Strong<graphics::Buffer> rt_block_grid_buffer;
  // Byte offset of the material chunks within rt_block_grid_buffer; the
  // shapes start at 0. See rt.glsl.
  std::size_t rt_block_material_grid_offset;
  rc::Strong<graphics::Buffer> rt_entity_buffer;
  render_graph::Symbolic_buffer rt_entity_binning_buffer;
  std::size_t rt_entity_binning_mask_offset;
  std::size_t rt_entity_binning_grid_offset;
  std::size_t rt_entity_binning_nodes_offset;
};

class Direct_trace_sun_pass : public render_graph::Node {
public:
  Direct_trace_sun_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_trace_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_trace_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _direct_irradiance_handle{};
};

class Direct_trace_sky_pass : public render_graph::Node {
public:
  Direct_trace_sky_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_trace_pass_inputs inputs,
    render_graph::Symbolic_image sky_view_lut);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_trace_pass_inputs _inputs;
  render_graph::Symbolic_image _sky_view_lut;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _direct_irradiance_handle{};
};

// Technique B of the MIS estimate: one cosine-weighted BRDF ray per valid
// pixel, accounting for every emitter along the direction it chose. See
// direct_trace_brdf.comp.
class Direct_trace_brdf_pass : public render_graph::Node {
public:
  Direct_trace_brdf_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_trace_pass_inputs inputs,
    render_graph::Symbolic_image sky_view_lut);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_trace_pass_inputs _inputs;
  render_graph::Symbolic_image _sky_view_lut;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _direct_irradiance_handle{};
};

// Reprojects and temporally accumulates the raw trace output against last
// frame's accumulated history -- see direct_temporal.comp.
// previous_* fields are unsymbolized: last frame's already-retired data,
// so no this-frame barrier applies. No gradient needed -- the
// history-reject check only compares depth and normal.
struct Direct_temporal_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  rc::Strong<graphics::Image const> previous_depth_render_target;
  rc::Strong<graphics::Image const> previous_normal_render_target;
  // Needed to reconstruct linear depth from the reverse-Z hardware depth
  // values above -- see gbuffer.glsl.
  float z_near;
  render_graph::Symbolic_image motion_vector_render_target;
  // One per MIS technique: A's picked ray and B's BRDF ray. Summed, not
  // averaged -- each technique estimates the whole integral.
  render_graph::Symbolic_image raw_direct_irradiance_render_target;
  render_graph::Symbolic_image raw_brdf_direct_irradiance_render_target;
  rc::Strong<graphics::Image const> previous_direct_irradiance_render_target;
  rc::Strong<graphics::Image const>
    previous_direct_luminance_render_target;
  u32 history_valid;
  render_graph::Symbolic_image direct_irradiance_render_target;
  render_graph::Symbolic_image direct_luminance_render_target;
  math::ivec2 framebuffer_size;
};

class Direct_temporal_pass : public render_graph::Node {
public:
  Direct_temporal_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_temporal_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_temporal_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _motion_vector_handle{};
  render_graph::Resource_handle _raw_direct_irradiance_handle{};
  render_graph::Resource_handle _raw_brdf_direct_irradiance_handle{};
  render_graph::Resource_handle _direct_irradiance_handle{};
  render_graph::Resource_handle _luminance_handle{};
};

struct Direct_variance_pass_inputs {
  // Only depth is needed (sky-mask check) -- normal/gradient aren't used
  // here.
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image direct_luminance_render_target;
  render_graph::Symbolic_image direct_variance_render_target;
  math::ivec2 framebuffer_size;
};

class Direct_variance_pass : public render_graph::Node {
public:
  Direct_variance_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_variance_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_variance_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _luminance_handle{};
  render_graph::Resource_handle _variance_handle{};
};

// One a-trous iteration; instantiated 5x (steps 1/2/4/8/16) with different
// in/out descriptor pairs each frame. The only pass that needs the
// gradient texture.
struct Direct_spatial_filter_pass_inputs {
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

class Direct_spatial_filter_pass : public render_graph::Node {
public:
  Direct_spatial_filter_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    u32 step_size,
    Direct_spatial_filter_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  u32 _step_size;
  Direct_spatial_filter_pass_inputs _inputs;
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
