#ifndef FPSPARTY_CLIENT_PASSES_SKY_VIEW_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_SKY_VIEW_PASS_HPP

#include "graphics/compute_pipeline.hpp"
#include "graphics/image.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"

namespace fpsparty::client::passes {

class Sky_view_pass : public render_graph::Node {
public:
  Sky_view_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline, math::ivec2 lut_size);

  // Called once per frame, before Graph::add_pass.
  //
  // transmittance_lut is unsymbolized: written once at startup, never
  // again, so no barrier is ever needed for it.
  void update(
    rc::Strong<graphics::Image const> transmittance_lut,
    render_graph::Symbolic_image sky_view_lut,
    math::vec3 camera_position,
    math::vec3 sun_direction,
    math::vec3 sun_irradiance);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  math::ivec2 _lut_size;
  rc::Strong<graphics::Image const> _transmittance_lut{};
  render_graph::Symbolic_image _sky_view_lut{};
  math::vec3 _camera_position{};
  math::vec3 _sun_direction{};
  math::vec3 _sun_irradiance{};
  render_graph::Resource_handle _sky_view_lut_handle{};
};

} // namespace fpsparty::client::passes

#endif
