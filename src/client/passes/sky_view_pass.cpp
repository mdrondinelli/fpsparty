#include "client/passes/sky_view_pass.hpp"
#include "render_graph/access.hpp"
#include <span>
#include <utility>

namespace fpsparty::client::passes {

Sky_view_pass::Sky_view_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline, math::ivec2 lut_size)
    : _pipeline{std::move(pipeline)}, _lut_size{lut_size} {}

void Sky_view_pass::update(
  rc::Strong<graphics::Image const> transmittance_lut,
  render_graph::Symbolic_image sky_view_lut,
  math::vec3 camera_position,
  math::vec3 sun_direction,
  math::vec3 sun_irradiance) {
  _transmittance_lut = std::move(transmittance_lut);
  _sky_view_lut = sky_view_lut;
  _camera_position = camera_position;
  _sun_direction = sun_direction;
  _sun_irradiance = sun_irradiance;
}

void Sky_view_pass::declare(render_graph::Builder &builder) {
  _sky_view_lut_handle =
    builder.write(_sky_view_lut, render_graph::access::compute_storage_write);
}

void Sky_view_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const camera_altitude = _camera_position.y();
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_descriptors(
    0,
    {{.image = _transmittance_lut, .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_sky_view_lut_handle),
      .kind = graphics::Descriptor_kind::storage}});
  recorder.push_data(4, std::as_bytes(std::span{&camera_altitude, 1}));
  recorder.push_data(16, std::as_bytes(std::span{&_sun_direction, 1}));
  recorder.push_data(32, std::as_bytes(std::span{&_sun_irradiance, 1}));
  recorder.dispatch(_lut_size.x(), _lut_size.y(), 1);
}

} // namespace fpsparty::client::passes
