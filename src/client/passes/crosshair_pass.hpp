#ifndef FPSPARTY_CLIENT_PASSES_CROSSHAIR_PASS_HPP
#define FPSPARTY_CLIENT_PASSES_CROSSHAIR_PASS_HPP

#include "graphics/buffer.hpp"
#include "graphics/pipeline.hpp"
#include "math/vec.hpp"
#include "rc.hpp"
#include "render_graph/node.hpp"
#include "render_graph/symbolic_resource.hpp"
#include <cstddef>

namespace fpsparty::client::passes {

class Crosshair_pass : public render_graph::Node {
public:
  Crosshair_pass(
    rc::Strong<graphics::Pipeline> pipeline, std::size_t index_count);

  // index_buffer isn't a constructor parameter even though it's just as
  // stable as pipeline -- unlike pipeline (assigned via this Application's
  // own constructor's member-initializer-list, so already valid by the
  // time this Node's default member initializer runs), it's assigned in
  // the constructor *body*, which runs after every member (including this
  // one) is already constructed -- capturing it by value up front would
  // silently capture an empty buffer forever.
  void update(
    rc::Strong<graphics::Buffer> index_buffer,
    render_graph::Symbolic_image mask_render_target,
    math::ivec2 framebuffer_size);

  void declare(render_graph::Builder &builder) override;

  void execute(
    graphics::Work_recorder &recorder,
    render_graph::Resources &resources) override;

private:
  rc::Strong<graphics::Pipeline> _pipeline;
  std::size_t _index_count;
  rc::Strong<graphics::Buffer> _index_buffer{};
  render_graph::Symbolic_image _mask_render_target{};
  math::ivec2 _framebuffer_size{};
  render_graph::Resource_handle _mask_handle{};
};

} // namespace fpsparty::client::passes

#endif
