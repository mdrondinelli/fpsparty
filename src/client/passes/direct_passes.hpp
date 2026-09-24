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

// Separate transfer pass so the graph orders count clearing before appends.
struct Direct_sample_clear_pass_inputs {
  render_graph::Symbolic_buffer sun_queue_buffer;
  render_graph::Symbolic_buffer sky_queue_buffer;
};

class Direct_sample_clear_pass : public render_graph::Node {
public:
  explicit Direct_sample_clear_pass(Direct_sample_clear_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  Direct_sample_clear_pass_inputs _inputs;
  render_graph::Resource_handle _sun_queue_handle{};
  render_graph::Resource_handle _sky_queue_handle{};
};

// Writes both samples' random variables to a per-pixel payload image and
// bins the environment ray by the lobe that drew it. The payload is all
// that is handed forward: p_cone and the lobe choice are recomputed by
// whoever needs them, so nothing downstream can read a stale weight.
//
// The BRDF ray gets no queue. It is unconditional over surface pixels and
// its directions are incoherent, so compaction is all a queue would buy.
struct Direct_sample_gen_pass_inputs {
  // Reverse-Z depth and oct-encoded normal; see gbuffer.glsl.
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  // Never written within any frame's graph (created once at startup) --
  // no symbol, held directly.
  rc::Strong<graphics::Image const> transmittance_lut;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  render_graph::Symbolic_image payload_render_target;
  render_graph::Symbolic_buffer sun_queue_buffer;
  render_graph::Symbolic_buffer sky_queue_buffer;
  math::ivec2 framebuffer_size;
  u32 frame_number;
};

class Direct_sample_gen_pass : public render_graph::Node {
public:
  Direct_sample_gen_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_sample_gen_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_sample_gen_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _payload_handle{};
  render_graph::Resource_handle _sun_queue_handle{};
  render_graph::Resource_handle _sky_queue_handle{};
};

// Turns the queues' atomic counts into indirect dispatch args, in place.
struct Direct_indirect_args_pass_inputs {
  render_graph::Symbolic_buffer sun_queue_buffer;
  render_graph::Symbolic_buffer sky_queue_buffer;
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
  render_graph::Resource_handle _sun_queue_handle{};
  render_graph::Resource_handle _sky_queue_handle{};
};

// Grid and entity data the traces walk. Unsymbolized: never written
// within any frame's graph, apart from the binning buffer.
struct Direct_rt_inputs {
  rc::Strong<graphics::Buffer> block_grid_buffer;
  // Byte offset of the material chunks within block_grid_buffer; the
  // shapes start at 0. See rt.glsl.
  std::size_t block_material_grid_offset;
  rc::Strong<graphics::Buffer> entity_buffer;
  render_graph::Symbolic_buffer entity_binning_buffer;
  std::size_t entity_binning_mask_offset;
  std::size_t entity_binning_grid_offset;
  std::size_t entity_binning_nodes_offset;
};

enum class Direct_trace_kind { sun, sky };

struct Direct_trace_variant {
  rc::Strong<graphics::Compute_pipeline> pipeline;
  render_graph::Symbolic_buffer queue;
};

struct Direct_trace_variants {
  Direct_trace_variant sun;
  Direct_trace_variant sky;
};

// Resources common to both environment traces.
struct Direct_trace_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  render_graph::Symbolic_image payload_render_target;
  render_graph::Symbolic_image environment_numerator_render_target;
  rc::Strong<graphics::Image const> transmittance_lut;
  render_graph::Symbolic_image sky_view_lut;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  Direct_rt_inputs rt;
};

// One environment lobe's trace, over its coherence bin. Writes the
// unweighted integrand; Direct_combine_pass does the division, so the two
// lobes partition the pixels and neither reads what the other wrote.
class Direct_trace_pass : public render_graph::Node {
public:
  Direct_trace_pass(
    Direct_trace_kind kind,
    Direct_trace_variants const &variants,
    Direct_trace_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_trace_pass_inputs _inputs;
  render_graph::Symbolic_buffer _queue;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _payload_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _queue_handle{};
  render_graph::Resource_handle _numerator_handle{};
};

// The BRDF trace. Dispatched over the whole screen rather than a queue --
// see direct_trace_brdf.comp -- and writes its own numerator target, so
// nothing orders it against the environment traces.
struct Direct_brdf_trace_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  render_graph::Symbolic_image payload_render_target;
  render_graph::Symbolic_image brdf_numerator_render_target;
  rc::Strong<graphics::Image const> transmittance_lut;
  render_graph::Symbolic_image sky_view_lut;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  Direct_rt_inputs rt;
  math::ivec2 framebuffer_size;
};

class Direct_brdf_trace_pass : public render_graph::Node {
public:
  Direct_brdf_trace_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_brdf_trace_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_brdf_trace_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _payload_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _numerator_handle{};
};

// Rebuilds both samples' directions and densities per pixel and applies
// the MIS division the traces skipped -- see direct_combine.comp.
struct Direct_combine_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  rc::Strong<graphics::Image const> transmittance_lut;
  render_graph::Symbolic_image payload_render_target;
  render_graph::Symbolic_image environment_numerator_render_target;
  render_graph::Symbolic_image brdf_numerator_render_target;
  render_graph::Symbolic_image raw_direct_irradiance_render_target;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  math::ivec2 framebuffer_size;
  u32 frame_number;
};

class Direct_combine_pass : public render_graph::Node {
public:
  Direct_combine_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline,
    Direct_combine_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Direct_combine_pass_inputs _inputs;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _payload_handle{};
  render_graph::Resource_handle _environment_numerator_handle{};
  render_graph::Resource_handle _brdf_numerator_handle{};
  render_graph::Resource_handle _raw_direct_irradiance_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
};

// Accumulate irradiance against previous-frame history. Previous images
// are held directly because they have no writers in this frame's graph.
struct Direct_temporal_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  rc::Strong<graphics::Image const> previous_depth_render_target;
  rc::Strong<graphics::Image const> previous_normal_render_target;
  // Needed to reconstruct linear depth from the reverse-Z hardware depth
  // values above -- see gbuffer.glsl.
  float z_near;
  render_graph::Symbolic_image motion_vector_render_target;
  render_graph::Symbolic_image raw_direct_irradiance_render_target;
  rc::Strong<graphics::Image const> previous_direct_irradiance_render_target;
  rc::Strong<graphics::Image const> previous_direct_luminance_render_target;
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
  render_graph::Resource_handle _direct_irradiance_handle{};
  render_graph::Resource_handle _luminance_handle{};
};

struct Direct_variance_pass_inputs {
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
