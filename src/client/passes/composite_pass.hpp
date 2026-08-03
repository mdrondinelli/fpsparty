#ifndef FPSPARTY_CLIENT_PASSES_COMPOSITE_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_COMPOSITE_PASS_HPP

#include "graphics/buffer.hpp"
#include "graphics/descriptor.hpp"
#include "graphics/image.hpp"
#include "graphics/pipeline.hpp"
#include "int.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include <cstddef>

namespace fpsparty::client::passes {

struct Composite_pass_inputs {
  // Neither pipeline (format-dependent, see Application::get_composite_
  // pipeline) nor index_buffer (assigned in Application's constructor
  // *body*, like Crosshair_pass's -- see its header comment) are stable
  // enough to be constructor parameters.
  rc::Strong<graphics::Pipeline> pipeline;
  rc::Strong<graphics::Buffer> index_buffer;
  rc::Strong<graphics::Image> swapchain_image;
  rc::Strong<graphics::Descriptor> radiance_descriptor;
  rc::Strong<graphics::Descriptor> crosshair_mask_descriptor;
  math::ivec2 framebuffer_size;
  u32 frame_number;
};

class Composite_pass : public render_graph::Node {
public:
  explicit Composite_pass(std::size_t index_count);

  void update(Composite_pass_inputs inputs);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  std::size_t _index_count;
  Composite_pass_inputs _inputs{};
  render_graph::Resource_handle _swapchain_handle{};
  render_graph::Resource_handle _radiance_handle{};
  render_graph::Resource_handle _crosshair_mask_handle{};
};

} // namespace fpsparty::client::passes

#endif
