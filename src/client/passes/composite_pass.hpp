#ifndef FPSPARTY_CLIENT_PASSES_COMPOSITE_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_COMPOSITE_PASS_HPP

#include "graphics/buffer.hpp"
#include "graphics/pipeline.hpp"
#include "int.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <cstddef>

namespace fpsparty::client::passes {

struct Composite_pass_inputs {
  // pipeline: format-dependent, see Application::get_composite_pipeline.
  // index_buffer: assigned in Application's constructor body -- see
  // Crosshair_pass's header comment.
  rc::Strong<graphics::Pipeline> pipeline;
  rc::Strong<graphics::Buffer> index_buffer;
  // No backing Application member: freshly acquired by Graphics::
  // record_frame_work every frame.
  render_graph::Symbolic_image swapchain_image;
  render_graph::Symbolic_image radiance_render_target;
  render_graph::Symbolic_image crosshair_mask_render_target;
  math::ivec2 framebuffer_size;
  u32 frame_number;
};

class Composite_pass : public render_graph::Node {
public:
  Composite_pass(std::size_t index_count, Composite_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  std::size_t _index_count;
  Composite_pass_inputs _inputs;
  render_graph::Resource_handle _swapchain_handle{};
  render_graph::Resource_handle _radiance_handle{};
  render_graph::Resource_handle _crosshair_mask_handle{};
};

} // namespace fpsparty::client::passes

#endif
