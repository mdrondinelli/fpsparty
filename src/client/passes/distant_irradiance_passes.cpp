#include "client/passes/distant_irradiance_passes.hpp"
#include "client/scene_uniform_layout.hpp"
#include "graphics/synchronization_scope.hpp"
#include "render_graph/access.hpp"
#include <span>
#include <utility>

namespace fpsparty::client::passes {

namespace {
auto constexpr transfer_write_scope = graphics::Synchronization_scope{
  .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
  .access_mask = graphics::Access_flag_bits::transfer_write,
};
auto constexpr compute_shader_storage_write_scope =
  graphics::Synchronization_scope{
    .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
    .access_mask = graphics::Access_flag_bits::shader_storage_write,
  };

math::ivec2 dispatch_group_count(math::ivec2 framebuffer_size) {
  return {
    (framebuffer_size.x() + 7) / 8,
    (framebuffer_size.y() + 7) / 8,
  };
}
} // namespace

Distant_irradiance_pass1::Distant_irradiance_pass1(
  rc::Strong<graphics::Compute_pipeline> pipeline)
    : _pipeline{std::move(pipeline)} {}

void Distant_irradiance_pass1::update(Distant_irradiance_pass1_inputs inputs) {
  _inputs = std::move(inputs);
}

void Distant_irradiance_pass1::declare(render_graph::Builder &builder) {
  // normal/depth: written by Gbuffer_pass. sky_view_lut: written by
  // Sky_view_pass. scene_uniform_buffer: written (GPU-side, the Sky_
  // irradiance sub-struct) by Sky_irradiance_pass -- whole-buffer
  // granularity, so this is conservative about the rest of the buffer
  // (gbuffer's own writes into it are host-side, no GPU barrier needed for
  // those). All three are only written this frame if this pass is running
  // at all (see the camera/sun/grid_mesh gating around every add_pass call
  // for these), so declaring the read even when there's nothing to
  // barrier against this particular frame is harmless -- Graph just won't
  // find a same-batch writer and won't insert one.
  builder.read(
    _inputs.normal_descriptor, render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.depth_descriptor, render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.sky_view_lut_sampled_descriptor,
    render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  _sun_sample_handle = builder.write(
    _inputs.sun_sample_buffer, render_graph::access::compute_storage_write);
  _sky_sample_handle = builder.write(
    _inputs.sky_sample_buffer, render_graph::access::compute_storage_write);
}

void Distant_irradiance_pass1::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &) {
  // Only the atomic counts need clearing (not the sample data itself) --
  // pass 2 only ever reads indices below whatever count this pass ends up
  // with. The count field sits 12 bytes in (see get_distant_light_sample_
  // buffer).
  recorder.fill_buffer(_inputs.sun_sample_buffer, 12, 4, 0u);
  recorder.fill_buffer(_inputs.sky_sample_buffer, 12, 4, 0u);
  recorder.barrier(transfer_write_scope, compute_shader_storage_write_scope);
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, _inputs.scene_uniform_buffer, _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {_inputs.normal_descriptor,
     _inputs.depth_descriptor,
     _inputs.transmittance_lut_sampled_descriptor,
     _inputs.sky_view_lut_sampled_descriptor});
  recorder.push_buffer_reference(
    16,
    _inputs.scene_uniform_buffer,
    _inputs.scene_uniform_offset + scene_sky_irradiance_offset);
  recorder.push_buffer_reference(24, _inputs.sun_sample_buffer, 12);
  recorder.push_buffer_reference(32, _inputs.sky_sample_buffer, 12);
  recorder.push_data(
    40, std::as_bytes(std::span{&_inputs.frame_number, 1}));
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

Distant_irradiance_indirect_args_pass::Distant_irradiance_indirect_args_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline)
    : _pipeline{std::move(pipeline)} {}

void Distant_irradiance_indirect_args_pass::update(
  Distant_irradiance_indirect_args_pass_inputs inputs) {
  _inputs = std::move(inputs);
}

void Distant_irradiance_indirect_args_pass::declare(
  render_graph::Builder &builder) {
  builder.read(
    _inputs.sun_sample_buffer, render_graph::access::compute_storage_read);
  builder.read(
    _inputs.sky_sample_buffer, render_graph::access::compute_storage_read);
  _sun_sample_handle = builder.write(
    _inputs.sun_sample_buffer, render_graph::access::compute_storage_write);
  _sky_sample_handle = builder.write(
    _inputs.sky_sample_buffer, render_graph::access::compute_storage_write);
}

void Distant_irradiance_indirect_args_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(0, _inputs.sun_sample_buffer);
  recorder.dispatch(1, 1, 1);
  recorder.push_buffer_reference(0, _inputs.sky_sample_buffer);
  recorder.dispatch(1, 1, 1);
}

namespace {
auto constexpr trace_read_access = render_graph::Access{
  .stage_mask = render_graph::access::indirect_command_read.stage_mask |
                render_graph::access::compute_storage_read.stage_mask,
  .access_mask = render_graph::access::indirect_command_read.access_mask |
                 render_graph::access::compute_storage_read.access_mask,
};
} // namespace

Distant_irradiance_trace_sun_pass::Distant_irradiance_trace_sun_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline)
    : _pipeline{std::move(pipeline)} {}

void Distant_irradiance_trace_sun_pass::update(
  Distant_irradiance_trace_pass_inputs inputs) {
  _inputs = std::move(inputs);
}

void Distant_irradiance_trace_sun_pass::declare(render_graph::Builder &builder) {
  // See Distant_irradiance_pass1::declare's comment -- same reasoning for
  // all of these (Gbuffer_pass/Sky_irradiance_pass/Rt_entity_binning_pass
  // writes).
  builder.read(
    _inputs.normal_descriptor, render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.depth_descriptor, render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.motion_vector_descriptor,
    render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  builder.read(
    _inputs.rt_entity_binning_buffer,
    render_graph::access::compute_storage_read);
  _sample_handle = builder.read(_inputs.sample_buffer, trace_read_access);
  _distant_irradiance_handle = builder.write(
    _inputs.distant_irradiance_storage_descriptor,
    render_graph::access::compute_storage_write);
  _luminance_handle = builder.write(
    _inputs.luminance_storage_descriptor,
    render_graph::access::compute_storage_write);
}

void Distant_irradiance_trace_sun_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, _inputs.scene_uniform_buffer, _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {_inputs.normal_descriptor,
     _inputs.depth_descriptor,
     _inputs.distant_irradiance_storage_descriptor,
     _inputs.transmittance_lut_sampled_descriptor});
  recorder.push_buffer_reference(
    16,
    _inputs.scene_uniform_buffer,
    _inputs.scene_uniform_offset + scene_sky_irradiance_offset);
  recorder.push_buffer_reference(24, _inputs.sample_buffer, 12);
  recorder.push_buffer_reference(32, _inputs.rt_block_grid_buffer);
  recorder.push_buffer_reference(40, _inputs.rt_entity_buffer);
  recorder.push_buffer_reference(
    48, _inputs.rt_entity_binning_buffer, _inputs.rt_entity_binning_grid_offset);
  recorder.push_buffer_reference(
    56,
    _inputs.rt_entity_binning_buffer,
    _inputs.rt_entity_binning_nodes_offset);
  recorder.push_descriptors(
    64,
    {_inputs.previous_depth_descriptor,
     _inputs.previous_distant_irradiance_descriptor,
     _inputs.motion_vector_descriptor,
     _inputs.previous_normal_descriptor});
  recorder.push_data(
    72, std::as_bytes(std::span{&_inputs.history_valid, 1}));
  recorder.push_descriptors(
    76,
    {_inputs.luminance_storage_descriptor,
     _inputs.previous_luminance_descriptor});
  recorder.dispatch_indirect({.buffer = _inputs.sample_buffer, .offset = 0});
}

Distant_irradiance_trace_sky_pass::Distant_irradiance_trace_sky_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline)
    : _pipeline{std::move(pipeline)} {}

void Distant_irradiance_trace_sky_pass::update(
  Distant_irradiance_trace_pass_inputs inputs,
  rc::Strong<graphics::Descriptor> sky_view_lut_sampled_descriptor) {
  _inputs = std::move(inputs);
  _sky_view_lut_sampled_descriptor = std::move(sky_view_lut_sampled_descriptor);
}

void Distant_irradiance_trace_sky_pass::declare(render_graph::Builder &builder) {
  // See Distant_irradiance_pass1::declare's comment.
  builder.read(
    _inputs.normal_descriptor, render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.depth_descriptor, render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.motion_vector_descriptor,
    render_graph::access::compute_sampled_read);
  builder.read(
    _sky_view_lut_sampled_descriptor,
    render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  builder.read(
    _inputs.rt_entity_binning_buffer,
    render_graph::access::compute_storage_read);
  _sample_handle = builder.read(_inputs.sample_buffer, trace_read_access);
  _distant_irradiance_handle = builder.write(
    _inputs.distant_irradiance_storage_descriptor,
    render_graph::access::compute_storage_write);
  _luminance_handle = builder.write(
    _inputs.luminance_storage_descriptor,
    render_graph::access::compute_storage_write);
}

void Distant_irradiance_trace_sky_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, _inputs.scene_uniform_buffer, _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {_inputs.normal_descriptor,
     _inputs.depth_descriptor,
     _inputs.distant_irradiance_storage_descriptor,
     _inputs.transmittance_lut_sampled_descriptor,
     _sky_view_lut_sampled_descriptor});
  recorder.push_buffer_reference(
    24,
    _inputs.scene_uniform_buffer,
    _inputs.scene_uniform_offset + scene_sky_irradiance_offset);
  recorder.push_buffer_reference(32, _inputs.sample_buffer, 12);
  recorder.push_buffer_reference(40, _inputs.rt_block_grid_buffer);
  recorder.push_buffer_reference(48, _inputs.rt_entity_buffer);
  recorder.push_buffer_reference(
    56, _inputs.rt_entity_binning_buffer, _inputs.rt_entity_binning_grid_offset);
  recorder.push_buffer_reference(
    64,
    _inputs.rt_entity_binning_buffer,
    _inputs.rt_entity_binning_nodes_offset);
  recorder.push_descriptors(
    72,
    {_inputs.previous_depth_descriptor,
     _inputs.previous_distant_irradiance_descriptor,
     _inputs.motion_vector_descriptor,
     _inputs.previous_normal_descriptor});
  recorder.push_data(
    80, std::as_bytes(std::span{&_inputs.history_valid, 1}));
  recorder.push_descriptors(
    84,
    {_inputs.luminance_storage_descriptor,
     _inputs.previous_luminance_descriptor});
  recorder.dispatch_indirect({.buffer = _inputs.sample_buffer, .offset = 0});
}

Distant_irradiance_variance_pass::Distant_irradiance_variance_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline)
    : _pipeline{std::move(pipeline)} {}

void Distant_irradiance_variance_pass::update(
  Distant_irradiance_variance_pass_inputs inputs) {
  _inputs = std::move(inputs);
}

void Distant_irradiance_variance_pass::declare(render_graph::Builder &builder) {
  // depth: written by Gbuffer_pass -- see Distant_irradiance_pass1::
  // declare's comment.
  builder.read(
    _inputs.depth_descriptor, render_graph::access::compute_sampled_read);
  _luminance_handle = builder.read(
    _inputs.luminance_descriptor, render_graph::access::compute_sampled_read);
  _variance_handle = builder.write(
    _inputs.variance_storage_descriptor,
    render_graph::access::compute_storage_write);
}

void Distant_irradiance_variance_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(
    0,
    {_inputs.depth_descriptor,
     _inputs.luminance_descriptor,
     _inputs.variance_storage_descriptor});
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

Distant_irradiance_spatial_filter_pass::Distant_irradiance_spatial_filter_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline, u32 step_size)
    : _pipeline{std::move(pipeline)}, _step_size{step_size} {}

void Distant_irradiance_spatial_filter_pass::update(
  Distant_irradiance_spatial_filter_pass_inputs inputs) {
  _inputs = std::move(inputs);
}

void Distant_irradiance_spatial_filter_pass::declare(
  render_graph::Builder &builder) {
  // depth/normal/depth_gradient: written by Gbuffer_pass -- see Distant_
  // irradiance_pass1::declare's comment.
  builder.read(
    _inputs.depth_descriptor, render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.normal_descriptor, render_graph::access::compute_sampled_read);
  builder.read(
    _inputs.depth_gradient_descriptor,
    render_graph::access::compute_sampled_read);
  _color_in_handle = builder.read(
    _inputs.color_in_descriptor, render_graph::access::compute_sampled_read);
  _variance_in_handle = builder.read(
    _inputs.variance_in_descriptor,
    render_graph::access::compute_sampled_read);
  _color_out_handle = builder.write(
    _inputs.color_out_storage_descriptor,
    render_graph::access::compute_storage_write);
  _variance_out_handle = builder.write(
    _inputs.variance_out_storage_descriptor,
    render_graph::access::compute_storage_write);
}

void Distant_irradiance_spatial_filter_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(
    0,
    {_inputs.depth_descriptor,
     _inputs.normal_descriptor,
     _inputs.depth_gradient_descriptor,
     _inputs.color_in_descriptor,
     _inputs.variance_in_descriptor,
     _inputs.color_out_storage_descriptor,
     _inputs.variance_out_storage_descriptor});
  recorder.push_data(16, std::as_bytes(std::span{&_step_size, 1}));
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

} // namespace fpsparty::client::passes
