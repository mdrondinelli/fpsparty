#include "client/passes/sky_view_pass.hpp"
#include "render_graph/access.hpp"
#include <span>
#include <utility>

namespace fpsparty::client::passes {

Sky_view_pass::Sky_view_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline, math::ivec2 lut_size)
    : _pipeline{std::move(pipeline)}, _lut_size{lut_size} {}

void Sky_view_pass::update(
  rc::Strong<graphics::Descriptor> transmittance_lut_sampled_descriptor,
  rc::Strong<graphics::Descriptor> sky_view_lut_storage_descriptor,
  math::vec3 camera_position,
  math::vec3 sun_direction,
  math::vec3 sun_irradiance) {
  _transmittance_lut_sampled_descriptor =
    std::move(transmittance_lut_sampled_descriptor);
  _sky_view_lut_storage_descriptor =
    std::move(sky_view_lut_storage_descriptor);
  _camera_position = camera_position;
  _sun_direction = sun_direction;
  _sun_irradiance = sun_irradiance;
}

void Sky_view_pass::declare(render_graph::Builder &builder) {
  _transmittance_lut_handle = builder.read(
    _transmittance_lut_sampled_descriptor,
    render_graph::access::compute_sampled_read);
  _sky_view_lut_handle = builder.write(
    _sky_view_lut_storage_descriptor,
    render_graph::access::compute_storage_write);
}

void Sky_view_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const camera_altitude = _camera_position.y();
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(
    0,
    {resources.get_descriptor(_transmittance_lut_handle),
     resources.get_descriptor(_sky_view_lut_handle)});
  recorder.push_data(4, std::as_bytes(std::span{&camera_altitude, 1}));
  recorder.push_data(16, std::as_bytes(std::span{&_sun_direction, 1}));
  recorder.push_data(32, std::as_bytes(std::span{&_sun_irradiance, 1}));
  recorder.dispatch(_lut_size.x(), _lut_size.y(), 1);
}

} // namespace fpsparty::client::passes
