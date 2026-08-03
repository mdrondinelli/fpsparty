#include "client/passes/sky_irradiance_pass.hpp"
#include "render_graph/access.hpp"
#include <span>
#include <utility>

namespace fpsparty::client::passes {

Sky_irradiance_pass::Sky_irradiance_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline)
    : _pipeline{std::move(pipeline)} {}

void Sky_irradiance_pass::update(
  rc::Strong<graphics::Descriptor> sky_view_lut_sampled_descriptor,
  rc::Strong<graphics::Buffer> scene_uniform_buffer,
  float camera_altitude,
  std::size_t scene_uniform_sky_irradiance_offset) {
  _sky_view_lut_sampled_descriptor =
    std::move(sky_view_lut_sampled_descriptor);
  _scene_uniform_buffer = std::move(scene_uniform_buffer);
  _camera_altitude = camera_altitude;
  _scene_uniform_sky_irradiance_offset = scene_uniform_sky_irradiance_offset;
}

void Sky_irradiance_pass::declare(render_graph::Builder &builder) {
  _sky_view_lut_handle = builder.read(
    _sky_view_lut_sampled_descriptor, render_graph::access::compute_sampled_read);
  _scene_uniform_buffer_handle = builder.write(
    _scene_uniform_buffer, render_graph::access::compute_storage_write);
}

void Sky_irradiance_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(0, {resources.get_descriptor(_sky_view_lut_handle)});
  recorder.push_data(4, std::as_bytes(std::span{&_camera_altitude, 1}));
  recorder.push_buffer_reference(
    8, _scene_uniform_buffer, _scene_uniform_sky_irradiance_offset);
  recorder.dispatch(6, 1, 1);
}

} // namespace fpsparty::client::passes
