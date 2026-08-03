#ifndef FPSPARTY_CLIENT_PASSES_DISTANT_IRRADIANCE_PASSES_HPP
#define FPSPARTY_CLIENT_PASSES_DISTANT_IRRADIANCE_PASSES_HPP

#include "graphics/buffer.hpp"
#include "graphics/compute_pipeline.hpp"
#include "graphics/descriptor.hpp"
#include "int.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include <cstddef>

namespace fpsparty::client::passes {

// Pass 1: RIS-picks sun vs sky per pixel, appending each pixel to
// whichever of the two sample buffers it picked (their atomic counts are
// zeroed here too, immediately before the dispatch that appends to them --
// an internal detail, not a cross-pass dependency, so not declared).
struct Distant_irradiance_pass1_inputs {
  rc::Strong<graphics::Descriptor> normal_descriptor;
  rc::Strong<graphics::Descriptor> depth_descriptor;
  rc::Strong<graphics::Descriptor> transmittance_lut_sampled_descriptor;
  rc::Strong<graphics::Descriptor> sky_view_lut_sampled_descriptor;
  rc::Strong<graphics::Buffer> scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  rc::Strong<graphics::Buffer> sun_sample_buffer;
  rc::Strong<graphics::Buffer> sky_sample_buffer;
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
  render_graph::Resource_handle _sun_sample_handle{};
  render_graph::Resource_handle _sky_sample_handle{};
};

// Turns pass 1's atomic sample counts into indirect dispatch args for the
// trace passes below, in place in the same two buffers.
struct Distant_irradiance_indirect_args_pass_inputs {
  rc::Strong<graphics::Buffer> sun_sample_buffer;
  rc::Strong<graphics::Buffer> sky_sample_buffer;
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

// Shared by both trace passes below.
struct Distant_irradiance_trace_pass_inputs {
  rc::Strong<graphics::Descriptor> normal_descriptor;
  rc::Strong<graphics::Descriptor> depth_descriptor;
  rc::Strong<graphics::Descriptor> distant_irradiance_storage_descriptor;
  rc::Strong<graphics::Descriptor> transmittance_lut_sampled_descriptor;
  rc::Strong<graphics::Buffer> scene_uniform_buffer;
  std::size_t scene_uniform_offset;
  rc::Strong<graphics::Buffer> sample_buffer;
  rc::Strong<graphics::Buffer> rt_block_grid_buffer;
  rc::Strong<graphics::Buffer> rt_entity_buffer;
  rc::Strong<graphics::Buffer> rt_entity_binning_buffer;
  std::size_t rt_entity_binning_grid_offset;
  std::size_t rt_entity_binning_nodes_offset;
  rc::Strong<graphics::Descriptor> previous_depth_descriptor;
  rc::Strong<graphics::Descriptor> previous_distant_irradiance_descriptor;
  rc::Strong<graphics::Descriptor> motion_vector_descriptor;
  rc::Strong<graphics::Descriptor> previous_normal_descriptor;
  u32 history_valid;
  rc::Strong<graphics::Descriptor> luminance_storage_descriptor;
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
    rc::Strong<graphics::Descriptor> sky_view_lut_sampled_descriptor);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  Distant_irradiance_trace_pass_inputs _inputs{};
  rc::Strong<graphics::Descriptor> _sky_view_lut_sampled_descriptor{};
  render_graph::Resource_handle _sample_handle{};
  render_graph::Resource_handle _distant_irradiance_handle{};
  render_graph::Resource_handle _luminance_handle{};
};

struct Distant_irradiance_variance_pass_inputs {
  rc::Strong<graphics::Descriptor> depth_descriptor;
  rc::Strong<graphics::Descriptor> luminance_descriptor;
  rc::Strong<graphics::Descriptor> variance_storage_descriptor;
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
  render_graph::Resource_handle _luminance_handle{};
  render_graph::Resource_handle _variance_handle{};
};

// One a-trous iteration; instantiated 5x (steps 1/2/4/8/16) with different
// in/out descriptor pairs each frame.
struct Distant_irradiance_spatial_filter_pass_inputs {
  rc::Strong<graphics::Descriptor> depth_descriptor;
  rc::Strong<graphics::Descriptor> normal_descriptor;
  rc::Strong<graphics::Descriptor> depth_gradient_descriptor;
  rc::Strong<graphics::Descriptor> color_in_descriptor;
  rc::Strong<graphics::Descriptor> variance_in_descriptor;
  rc::Strong<graphics::Descriptor> color_out_storage_descriptor;
  rc::Strong<graphics::Descriptor> variance_out_storage_descriptor;
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
  render_graph::Resource_handle _color_in_handle{};
  render_graph::Resource_handle _variance_in_handle{};
  render_graph::Resource_handle _color_out_handle{};
  render_graph::Resource_handle _variance_out_handle{};
};

} // namespace fpsparty::client::passes

#endif
