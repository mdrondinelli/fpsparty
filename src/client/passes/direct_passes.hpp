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
  render_graph::Symbolic_buffer sun_sample_buffer;
  render_graph::Symbolic_buffer sky_sample_buffer;
  render_graph::Symbolic_buffer brdf_sample_buffer;
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
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
  render_graph::Resource_handle _brdf_sample_handle{};
};

// Generate one environment sample and one BRDF sample per surface pixel.
// Pixels without an environment sample have their raw irradiance cleared here.
struct Direct_sample_gen_pass_inputs {
  // Reverse-Z depth and oct-encoded normal; see gbuffer.glsl.
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
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
  render_graph::Resource_handle _brdf_sample_handle{};
};

// Turns the sample-generation pass's atomic sample counts into indirect
// dispatch args for the trace passes below, in place in the same three
// buffers.
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

enum class Direct_trace_kind { sun, sky, brdf };

struct Direct_trace_variant {
  rc::Strong<graphics::Compute_pipeline> pipeline;
  render_graph::Symbolic_buffer samples;
};

struct Direct_trace_variants {
  Direct_trace_variant sun;
  Direct_trace_variant sky;
  Direct_trace_variant brdf;
};

// Resources common to all trace variants.
struct Direct_trace_pass_inputs {
  render_graph::Symbolic_image depth_render_target;
  render_graph::Symbolic_image normal_render_target;
  render_graph::Symbolic_image raw_direct_irradiance_render_target;
  rc::Strong<graphics::Image const> transmittance_lut;
  render_graph::Symbolic_image sky_view_lut;
  render_graph::Symbolic_buffer scene_uniform_buffer;
  std::size_t scene_uniform_offset;
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

// The trace kind selects its pipeline, sample queue, and resource access.
// Environment traces initialize irradiance; the BRDF trace adds to it.
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
  Direct_trace_kind _kind;
  render_graph::Symbolic_buffer _samples;
  render_graph::Resource_handle _depth_handle{};
  render_graph::Resource_handle _normal_handle{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_handle{};
  render_graph::Resource_handle _rt_entity_binning_handle{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _direct_irradiance_handle{};
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
