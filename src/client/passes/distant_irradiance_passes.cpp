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
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Distant_irradiance_pass1_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Distant_irradiance_pass1::declare(render_graph::Builder &builder) {
  // normal/depth: written by Gbuffer_pass. sky_view_lut: written by
  // Sky_view_pass. scene_uniform_buffer: written GPU-side (the Sky_
  // irradiance sub-struct) by Sky_irradiance_pass, declared at whole-
  // buffer granularity. Declaring these reads is a no-op on frames where
  // the writer didn't run this batch -- Graph finds no matching writer
  // and skips the barrier.
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _sky_view_lut_handle = builder.read(
    _inputs.sky_view_lut, render_graph::access::compute_sampled_read);
  _scene_uniform_handle = builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  _sun_sample_handle = builder.write(
    _inputs.sun_sample_buffer, render_graph::access::compute_storage_write);
  _sky_sample_handle = builder.write(
    _inputs.sky_sample_buffer, render_graph::access::compute_storage_write);
}

void Distant_irradiance_pass1::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const &scene_uniform_buffer = resources.get_buffer(_scene_uniform_handle);
  auto const &sun_sample_buffer = resources.get_buffer(_sun_sample_handle);
  auto const &sky_sample_buffer = resources.get_buffer(_sky_sample_handle);
  // Only the atomic counts need clearing (not the sample data itself) --
  // pass 2 only ever reads indices below whatever count this pass ends up
  // with. The count field sits 12 bytes in (see get_distant_light_sample_
  // buffer).
  recorder.fill_buffer(sun_sample_buffer, 12, 4, 0u);
  recorder.fill_buffer(sky_sample_buffer, 12, 4, 0u);
  recorder.barrier(transfer_write_scope, compute_shader_storage_write_scope);
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, scene_uniform_buffer, _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {{.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.transmittance_lut,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_sky_view_lut_handle),
      .kind = graphics::Descriptor_kind::sampled}});
  recorder.push_buffer_reference(
    16,
    scene_uniform_buffer,
    _inputs.scene_uniform_offset + scene_sky_irradiance_offset);
  recorder.push_buffer_reference(24, sun_sample_buffer, 12);
  recorder.push_buffer_reference(32, sky_sample_buffer, 12);
  recorder.push_data(
    40, std::as_bytes(std::span{&_inputs.frame_number, 1}));
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

Distant_irradiance_indirect_args_pass::Distant_irradiance_indirect_args_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Distant_irradiance_indirect_args_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

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
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(0, resources.get_buffer(_sun_sample_handle));
  recorder.dispatch(1, 1, 1);
  recorder.push_buffer_reference(0, resources.get_buffer(_sky_sample_handle));
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
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Distant_irradiance_trace_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Distant_irradiance_trace_sun_pass::declare(render_graph::Builder &builder) {
  // See Distant_irradiance_pass1::declare's comment -- same reasoning for
  // all of these (Gbuffer_pass/Sky_irradiance_pass/Rt_entity_binning_pass
  // writes).
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _motion_vector_handle = builder.read(
    _inputs.motion_vector_render_target,
    render_graph::access::compute_sampled_read);
  _scene_uniform_handle = builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  _rt_entity_binning_handle = builder.read(
    _inputs.rt_entity_binning_buffer,
    render_graph::access::compute_storage_read);
  _sample_handle = builder.read(_inputs.sample_buffer, trace_read_access);
  _distant_irradiance_handle = builder.write(
    _inputs.distant_irradiance_render_target,
    render_graph::access::compute_storage_write);
  _luminance_handle = builder.write(
    _inputs.distant_irradiance_luminance_render_target,
    render_graph::access::compute_storage_write);
}

void Distant_irradiance_trace_sun_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const &scene_uniform_buffer = resources.get_buffer(_scene_uniform_handle);
  auto const &sample_buffer = resources.get_buffer(_sample_handle);
  auto const &rt_entity_binning_buffer =
    resources.get_buffer(_rt_entity_binning_handle);
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, scene_uniform_buffer, _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {{.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_distant_irradiance_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = _inputs.transmittance_lut,
      .kind = graphics::Descriptor_kind::sampled}});
  recorder.push_buffer_reference(
    16,
    scene_uniform_buffer,
    _inputs.scene_uniform_offset + scene_sky_irradiance_offset);
  recorder.push_buffer_reference(24, sample_buffer, 12);
  recorder.push_buffer_reference(32, _inputs.rt_block_grid_buffer);
  recorder.push_buffer_reference(40, _inputs.rt_entity_buffer);
  recorder.push_buffer_reference(
    48, rt_entity_binning_buffer, _inputs.rt_entity_binning_grid_offset);
  recorder.push_buffer_reference(
    56, rt_entity_binning_buffer, _inputs.rt_entity_binning_nodes_offset);
  recorder.push_descriptors(
    64,
    {{.image = _inputs.previous_depth_render_target,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.previous_distant_irradiance_render_target,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_motion_vector_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.previous_normal_render_target,
      .kind = graphics::Descriptor_kind::sampled}});
  recorder.push_data(
    72, std::as_bytes(std::span{&_inputs.history_valid, 1}));
  recorder.push_descriptors(
    76,
    {{.image = resources.get_image(_luminance_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = _inputs.previous_distant_irradiance_luminance_render_target,
      .kind = graphics::Descriptor_kind::sampled}});
  recorder.dispatch_indirect({.buffer = sample_buffer, .offset = 0});
}

Distant_irradiance_trace_sky_pass::Distant_irradiance_trace_sky_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Distant_irradiance_trace_pass_inputs inputs,
  render_graph::Symbolic_image sky_view_lut)
    : _pipeline{std::move(pipeline)},
      _inputs{std::move(inputs)},
      _sky_view_lut{sky_view_lut} {}

void Distant_irradiance_trace_sky_pass::declare(render_graph::Builder &builder) {
  // See Distant_irradiance_pass1::declare's comment.
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _motion_vector_handle = builder.read(
    _inputs.motion_vector_render_target,
    render_graph::access::compute_sampled_read);
  _sky_view_lut_handle = builder.read(
    _sky_view_lut, render_graph::access::compute_sampled_read);
  _scene_uniform_handle = builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  _rt_entity_binning_handle = builder.read(
    _inputs.rt_entity_binning_buffer,
    render_graph::access::compute_storage_read);
  _sample_handle = builder.read(_inputs.sample_buffer, trace_read_access);
  _distant_irradiance_handle = builder.write(
    _inputs.distant_irradiance_render_target,
    render_graph::access::compute_storage_write);
  _luminance_handle = builder.write(
    _inputs.distant_irradiance_luminance_render_target,
    render_graph::access::compute_storage_write);
}

void Distant_irradiance_trace_sky_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const &scene_uniform_buffer = resources.get_buffer(_scene_uniform_handle);
  auto const &sample_buffer = resources.get_buffer(_sample_handle);
  auto const &rt_entity_binning_buffer =
    resources.get_buffer(_rt_entity_binning_handle);
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, scene_uniform_buffer, _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {{.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_distant_irradiance_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = _inputs.transmittance_lut,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_sky_view_lut_handle),
      .kind = graphics::Descriptor_kind::sampled}});
  recorder.push_buffer_reference(
    24,
    scene_uniform_buffer,
    _inputs.scene_uniform_offset + scene_sky_irradiance_offset);
  recorder.push_buffer_reference(32, sample_buffer, 12);
  recorder.push_buffer_reference(40, _inputs.rt_block_grid_buffer);
  recorder.push_buffer_reference(48, _inputs.rt_entity_buffer);
  recorder.push_buffer_reference(
    56, rt_entity_binning_buffer, _inputs.rt_entity_binning_grid_offset);
  recorder.push_buffer_reference(
    64, rt_entity_binning_buffer, _inputs.rt_entity_binning_nodes_offset);
  recorder.push_descriptors(
    72,
    {{.image = _inputs.previous_depth_render_target,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.previous_distant_irradiance_render_target,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_motion_vector_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.previous_normal_render_target,
      .kind = graphics::Descriptor_kind::sampled}});
  recorder.push_data(
    80, std::as_bytes(std::span{&_inputs.history_valid, 1}));
  recorder.push_descriptors(
    84,
    {{.image = resources.get_image(_luminance_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = _inputs.previous_distant_irradiance_luminance_render_target,
      .kind = graphics::Descriptor_kind::sampled}});
  recorder.dispatch_indirect({.buffer = sample_buffer, .offset = 0});
}

Distant_irradiance_variance_pass::Distant_irradiance_variance_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Distant_irradiance_variance_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Distant_irradiance_variance_pass::declare(render_graph::Builder &builder) {
  // depth/normal: written by Gbuffer_pass -- see Distant_irradiance_
  // pass1::declare's comment.
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _depth_gradient_handle = builder.read(
    _inputs.depth_gradient_render_target,
    render_graph::access::compute_sampled_read);
  _luminance_handle = builder.read(
    _inputs.distant_irradiance_luminance_render_target,
    render_graph::access::compute_sampled_read);
  _variance_handle = builder.write(
    _inputs.distant_irradiance_variance_render_target,
    render_graph::access::compute_storage_write);
}

void Distant_irradiance_variance_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(
    0,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_depth_gradient_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_luminance_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_variance_handle),
      .kind = graphics::Descriptor_kind::storage}});
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

Distant_irradiance_spatial_filter_pass::Distant_irradiance_spatial_filter_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  u32 step_size,
  Distant_irradiance_spatial_filter_pass_inputs inputs)
    : _pipeline{std::move(pipeline)},
      _step_size{step_size},
      _inputs{std::move(inputs)} {}

void Distant_irradiance_spatial_filter_pass::declare(
  render_graph::Builder &builder) {
  // depth/normal/depth_gradient: written by Gbuffer_pass -- see Distant_
  // irradiance_pass1::declare's comment.
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _depth_gradient_handle = builder.read(
    _inputs.depth_gradient_render_target,
    render_graph::access::compute_sampled_read);
  _color_in_handle = builder.read(
    _inputs.color_in, render_graph::access::compute_sampled_read);
  _variance_in_handle = builder.read(
    _inputs.variance_in, render_graph::access::compute_sampled_read);
  _color_out_handle = builder.write(
    _inputs.color_out, render_graph::access::compute_storage_write);
  _variance_out_handle = builder.write(
    _inputs.variance_out, render_graph::access::compute_storage_write);
}

void Distant_irradiance_spatial_filter_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(
    0,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_depth_gradient_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_color_in_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_variance_in_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_color_out_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_variance_out_handle),
      .kind = graphics::Descriptor_kind::storage}});
  recorder.push_data(16, std::as_bytes(std::span{&_step_size, 1}));
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

} // namespace fpsparty::client::passes
