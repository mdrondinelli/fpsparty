#ifndef FPSPARTY_CLIENT_PASSES_SKY_IRRADIANCE_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_SKY_IRRADIANCE_PASS_HPP

#include "graphics/compute_pipeline.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <cstddef>

namespace fpsparty::client::passes {

// Writes into whatever byte range of scene_uniform_buffer the caller
// gives it (see Sky_irradiance_offset below) -- the CPU-side write to the
// rest of that frame's Scene happens later (Gbuffer_pass), on disjoint
// bytes, so no dependency between them; but later GPU reads of this
// pass's output (distant irradiance) do need the barrier Graph::execute
// now inserts automatically from the declared write/read.
class Sky_irradiance_pass : public render_graph::Node {
public:
  explicit Sky_irradiance_pass(
    rc::Strong<graphics::Compute_pipeline> pipeline);

  void update(
    render_graph::Symbolic_image sky_view_lut,
    render_graph::Symbolic_buffer scene_uniform_buffer,
    float camera_altitude,
    std::size_t scene_uniform_sky_irradiance_offset);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Compute_pipeline> _pipeline;
  render_graph::Symbolic_image _sky_view_lut{};
  render_graph::Symbolic_buffer _scene_uniform_buffer{};
  float _camera_altitude{};
  std::size_t _scene_uniform_sky_irradiance_offset{};
  render_graph::Resource_handle _sky_view_lut_handle{};
  render_graph::Resource_handle _scene_uniform_buffer_handle{};
};

} // namespace fpsparty::client::passes

#endif
