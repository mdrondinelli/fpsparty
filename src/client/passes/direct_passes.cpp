#include "client/passes/direct_passes.hpp"
#include "client/direct_sample_layout.hpp"
#include "client/scene_uniform_layout.hpp"
#include "render_graph/access.hpp"
#include <span>
#include <utility>

namespace fpsparty::client::passes {

namespace {
math::ivec2 dispatch_group_count(math::ivec2 framebuffer_size) {
  return {
    (framebuffer_size.x() + 7) / 8,
    (framebuffer_size.y() + 7) / 8,
  };
}
} // namespace

Direct_sample_clear_pass::Direct_sample_clear_pass(
  Direct_sample_clear_pass_inputs inputs)
    : _inputs{std::move(inputs)} {}

void Direct_sample_clear_pass::declare(render_graph::Builder &builder) {
  _sun_queue_handle = builder.write(
    _inputs.sun_queue_buffer, render_graph::access::transfer_write);
  _sky_queue_handle = builder.write(
    _inputs.sky_queue_buffer, render_graph::access::transfer_write);
  _first_state_handle = builder.write(
    _inputs.first_state_buffer, render_graph::access::transfer_write);
  _second_state_handle = builder.write(
    _inputs.second_state_buffer, render_graph::access::transfer_write);
}

void Direct_sample_clear_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  // Entries beyond the count are ignored by the trace shaders.
  recorder.fill_buffer(
    resources.get_buffer(_sun_queue_handle),
    direct_sample_count_offset,
    direct_sample_count_size,
    0u);
  recorder.fill_buffer(
    resources.get_buffer(_sky_queue_handle),
    direct_sample_count_offset,
    direct_sample_count_size,
    0u);
  // The continuation queues write their own dispatch command as they
  // fill, by growing x monotonically, so x starts at zero and y and z are
  // simply held at one. States past the count are never read.
  for (auto const handle : {_first_state_handle, _second_state_handle}) {
    auto const &buffer = resources.get_buffer(handle);
    recorder.fill_buffer(buffer, 0, 4, 0u);
    recorder.fill_buffer(buffer, 4, 8, 1u);
    recorder.fill_buffer(
      buffer, direct_sample_count_offset, direct_sample_count_size, 0u);
  }
}

Direct_sample_gen_pass::Direct_sample_gen_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Direct_sample_gen_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Direct_sample_gen_pass::declare(render_graph::Builder &builder) {
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _scene_uniform_handle = builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  _environment_uv_handle = builder.write(
    _inputs.environment_uv_render_target,
    render_graph::access::compute_storage_write);
  _brdf_uv_handle = builder.write(
    _inputs.brdf_uv_render_target,
    render_graph::access::compute_storage_write);
  _sun_queue_handle = builder.write(
    _inputs.sun_queue_buffer, render_graph::access::compute_storage_write);
  _sky_queue_handle = builder.write(
    _inputs.sky_queue_buffer, render_graph::access::compute_storage_write);
}

void Direct_sample_gen_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const &scene_uniform_buffer =
    resources.get_buffer(_scene_uniform_handle);
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, scene_uniform_buffer, _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.transmittance_lut,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_environment_uv_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_brdf_uv_handle),
      .kind = graphics::Descriptor_kind::storage}});
  recorder.push_buffer_reference(
    24,
    scene_uniform_buffer,
    _inputs.scene_uniform_offset + scene_sky_irradiance_offset);
  recorder.push_buffer_reference(
    32, resources.get_buffer(_sun_queue_handle), direct_sample_count_offset);
  recorder.push_buffer_reference(
    40, resources.get_buffer(_sky_queue_handle), direct_sample_count_offset);
  recorder.push_data(48, std::as_bytes(std::span{&_inputs.frame_number, 1}));
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

Direct_indirect_args_pass::Direct_indirect_args_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Direct_indirect_args_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Direct_indirect_args_pass::declare(render_graph::Builder &builder) {
  builder.read(
    _inputs.sun_queue_buffer, render_graph::access::compute_storage_read);
  builder.read(
    _inputs.sky_queue_buffer, render_graph::access::compute_storage_read);
  _sun_queue_handle = builder.write(
    _inputs.sun_queue_buffer, render_graph::access::compute_storage_write);
  _sky_queue_handle = builder.write(
    _inputs.sky_queue_buffer, render_graph::access::compute_storage_write);
}

void Direct_indirect_args_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(0, resources.get_buffer(_sun_queue_handle));
  recorder.dispatch(1, 1, 1);
  recorder.push_buffer_reference(0, resources.get_buffer(_sky_queue_handle));
  recorder.dispatch(1, 1, 1);
}

namespace {
auto constexpr trace_read_access = render_graph::Access{
  .stage_mask = render_graph::access::indirect_command_read.stage_mask |
                render_graph::access::compute_storage_read.stage_mask,
  .access_mask = render_graph::access::indirect_command_read.access_mask |
                 render_graph::access::compute_storage_read.access_mask,
};

// Both traces push the grid and entity references at the same offsets,
// counted from the first one -- the two blocks differ only in whether a
// queue reference sits ahead of them.
void push_rt_inputs(
  graphics::Work_recorder &recorder,
  Direct_rt_inputs const &rt,
  rc::Strong<graphics::Buffer> const &entity_binning_buffer,
  std::size_t base) {
  recorder.push_buffer_reference(base, rt.block_grid_buffer);
  recorder.push_buffer_reference(base + 8, rt.entity_buffer);
  recorder.push_buffer_reference(
    base + 16, entity_binning_buffer, rt.entity_binning_grid_offset);
  recorder.push_buffer_reference(
    base + 24, entity_binning_buffer, rt.entity_binning_nodes_offset);
  recorder.push_buffer_reference(
    base + 32, rt.block_grid_buffer, rt.block_material_grid_offset);
  recorder.push_buffer_reference(
    base + 40, entity_binning_buffer, rt.entity_binning_mask_offset);
}
} // namespace

Direct_trace_pass::Direct_trace_pass(
  Direct_trace_kind kind,
  Direct_trace_variants const &variants,
  Direct_trace_pass_inputs inputs)
    : _pipeline{
        kind == Direct_trace_kind::sun ? variants.sun.pipeline
                                       : variants.sky.pipeline},
      _inputs{std::move(inputs)},
      _queue{
        kind == Direct_trace_kind::sun ? variants.sun.queue
                                       : variants.sky.queue} {}

void Direct_trace_pass::declare(render_graph::Builder &builder) {
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _uv_handle = builder.read(
    _inputs.environment_uv_render_target,
    render_graph::access::compute_storage_read);
  _sky_view_lut_handle = builder.read(
    _inputs.sky_view_lut, render_graph::access::compute_sampled_read);
  _scene_uniform_handle = builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  _rt_entity_binning_handle = builder.read(
    _inputs.rt.entity_binning_buffer,
    render_graph::access::compute_storage_read);
  _queue_handle = builder.read(_queue, trace_read_access);
  _numerator_handle = builder.write(
    _inputs.environment_numerator_render_target,
    render_graph::access::compute_storage_write);
}

void Direct_trace_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const &queue_buffer = resources.get_buffer(_queue_handle);
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0,
    resources.get_buffer(_scene_uniform_handle),
    _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_uv_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_numerator_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = _inputs.transmittance_lut,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_sky_view_lut_handle),
      .kind = graphics::Descriptor_kind::sampled}});
  recorder.push_buffer_reference(
    24, queue_buffer, direct_sample_count_offset);
  push_rt_inputs(
    recorder,
    _inputs.rt,
    resources.get_buffer(_rt_entity_binning_handle),
    32);
  recorder.dispatch_indirect({.buffer = queue_buffer, .offset = 0});
}

Direct_brdf_trace_pass::Direct_brdf_trace_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Direct_brdf_trace_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Direct_brdf_trace_pass::declare(render_graph::Builder &builder) {
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _uv_handle = builder.read(
    _inputs.brdf_uv_render_target,
    render_graph::access::compute_storage_read);
  _sky_view_lut_handle = builder.read(
    _inputs.sky_view_lut, render_graph::access::compute_sampled_read);
  _scene_uniform_handle = builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  _rt_entity_binning_handle = builder.read(
    _inputs.rt.entity_binning_buffer,
    render_graph::access::compute_storage_read);
  _numerator_handle = builder.write(
    _inputs.brdf_numerator_render_target,
    render_graph::access::compute_storage_write);
  if (_inputs.is_continuation) {
    _in_state_handle = builder.read(_inputs.in_state_buffer, trace_read_access);
  }
  _out_state_handle = builder.write(
    _inputs.out_state_buffer, render_graph::access::compute_storage_write);
}

void Direct_brdf_trace_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0,
    resources.get_buffer(_scene_uniform_handle),
    _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_uv_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_numerator_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = _inputs.transmittance_lut,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_sky_view_lut_handle),
      .kind = graphics::Descriptor_kind::sampled}});
  push_rt_inputs(
    recorder,
    _inputs.rt,
    resources.get_buffer(_rt_entity_binning_handle),
    24);
  auto const &out_state_buffer = resources.get_buffer(_out_state_handle);
  auto const &in_state_buffer = _inputs.is_continuation
    ? resources.get_buffer(_in_state_handle)
    : out_state_buffer;
  recorder.push_buffer_reference(72, in_state_buffer);
  recorder.push_buffer_reference(80, out_state_buffer);
  auto const flags = static_cast<u32>(
    (_inputs.is_continuation ? 1u : 0u) | (_inputs.is_final ? 2u : 0u));
  recorder.push_data(88, std::as_bytes(std::span{&flags, 1}));
  if (_inputs.is_continuation) {
    recorder.dispatch_indirect({.buffer = in_state_buffer, .offset = 0});
  } else {
    auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
    recorder.dispatch(group_count.x(), group_count.y(), 1);
  }
}

Direct_combine_pass::Direct_combine_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Direct_combine_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Direct_combine_pass::declare(render_graph::Builder &builder) {
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _environment_uv_handle = builder.read(
    _inputs.environment_uv_render_target,
    render_graph::access::compute_storage_read);
  _brdf_uv_handle = builder.read(
    _inputs.brdf_uv_render_target,
    render_graph::access::compute_storage_read);
  _environment_numerator_handle = builder.read(
    _inputs.environment_numerator_render_target,
    render_graph::access::compute_storage_read);
  _brdf_numerator_handle = builder.read(
    _inputs.brdf_numerator_render_target,
    render_graph::access::compute_storage_read);
  _scene_uniform_handle = builder.read(
    _inputs.scene_uniform_buffer, render_graph::access::compute_storage_read);
  _raw_direct_irradiance_handle = builder.write(
    _inputs.raw_direct_irradiance_render_target,
    render_graph::access::compute_storage_write);
}

void Direct_combine_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const &scene_uniform_buffer =
    resources.get_buffer(_scene_uniform_handle);
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_buffer_reference(
    0, scene_uniform_buffer, _inputs.scene_uniform_offset);
  recorder.push_descriptors(
    8,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.transmittance_lut,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_environment_uv_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_brdf_uv_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_environment_numerator_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_brdf_numerator_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_raw_direct_irradiance_handle),
      .kind = graphics::Descriptor_kind::storage}});
  recorder.push_buffer_reference(
    24,
    scene_uniform_buffer,
    _inputs.scene_uniform_offset + scene_sky_irradiance_offset);
  recorder.push_data(32, std::as_bytes(std::span{&_inputs.frame_number, 1}));
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

Direct_temporal_pass::Direct_temporal_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Direct_temporal_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Direct_temporal_pass::declare(render_graph::Builder &builder) {
  // Previous-frame history is held directly; only current resources need
  // symbols.
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _motion_vector_handle = builder.read(
    _inputs.motion_vector_render_target,
    render_graph::access::compute_sampled_read);
  _raw_direct_irradiance_handle = builder.read(
    _inputs.raw_direct_irradiance_render_target,
    render_graph::access::compute_sampled_read);
  _direct_irradiance_handle = builder.write(
    _inputs.direct_irradiance_render_target,
    render_graph::access::compute_storage_write);
  _luminance_handle = builder.write(
    _inputs.direct_luminance_render_target,
    render_graph::access::compute_storage_write);
}

void Direct_temporal_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_data(0, std::as_bytes(std::span{&_inputs.history_valid, 1}));
  recorder.push_data(4, std::as_bytes(std::span{&_inputs.z_near, 1}));
  recorder.push_descriptors(
    8,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_raw_direct_irradiance_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.previous_depth_render_target,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.previous_normal_render_target,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.previous_direct_irradiance_render_target,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = _inputs.previous_direct_luminance_render_target,
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_motion_vector_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_direct_irradiance_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_luminance_handle),
      .kind = graphics::Descriptor_kind::storage}});
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

Direct_variance_pass::Direct_variance_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  Direct_variance_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

void Direct_variance_pass::declare(render_graph::Builder &builder) {
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _luminance_handle = builder.read(
    _inputs.direct_luminance_render_target,
    render_graph::access::compute_sampled_read);
  _variance_handle = builder.write(
    _inputs.direct_variance_render_target,
    render_graph::access::compute_storage_write);
}

void Direct_variance_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(
    0,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_luminance_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_variance_handle),
      .kind = graphics::Descriptor_kind::storage}});
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

Direct_spatial_filter_pass::Direct_spatial_filter_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline,
  u32 step_size,
  Direct_spatial_filter_pass_inputs inputs)
    : _pipeline{std::move(pipeline)},
      _step_size{step_size},
      _inputs{std::move(inputs)} {}

void Direct_spatial_filter_pass::declare(render_graph::Builder &builder) {
  _depth_handle = builder.read(
    _inputs.depth_render_target, render_graph::access::compute_sampled_read);
  _normal_handle = builder.read(
    _inputs.normal_render_target, render_graph::access::compute_sampled_read);
  _gradient_handle = builder.read(
    _inputs.gradient_render_target, render_graph::access::compute_sampled_read);
  _color_in_handle =
    builder.read(_inputs.color_in, render_graph::access::compute_sampled_read);
  _variance_in_handle = builder.read(
    _inputs.variance_in, render_graph::access::compute_sampled_read);
  _color_out_handle = builder.write(
    _inputs.color_out, render_graph::access::compute_storage_write);
  _variance_out_handle = builder.write(
    _inputs.variance_out, render_graph::access::compute_storage_write);
}

void Direct_spatial_filter_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(
    0,
    {{.image = resources.get_image(_depth_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_gradient_handle),
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
  recorder.push_data(20, std::as_bytes(std::span{&_inputs.z_near, 1}));
  auto const group_count = dispatch_group_count(_inputs.framebuffer_size);
  recorder.dispatch(group_count.x(), group_count.y(), 1);
}

} // namespace fpsparty::client::passes
