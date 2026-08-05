#include "client/passes/crosshair_pass.hpp"
#include "render_graph/access.hpp"
#include <array>
#include <span>
#include <utility>

namespace fpsparty::client::passes {

Crosshair_pass::Crosshair_pass(
  rc::Strong<graphics::Pipeline> pipeline,
  std::size_t index_count,
  rc::Strong<graphics::Buffer> index_buffer,
  render_graph::Symbolic_image mask_render_target,
  math::ivec2 framebuffer_size)
    : _pipeline{std::move(pipeline)},
      _index_count{index_count},
      _index_buffer{std::move(index_buffer)},
      _mask_render_target{mask_render_target},
      _framebuffer_size{framebuffer_size} {}

void Crosshair_pass::declare(render_graph::Builder &builder) {
  _mask_handle = builder.write(
    _mask_render_target, render_graph::access::color_attachment_write);
}

void Crosshair_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const color_attachments = std::array{
    graphics::Color_attachment_info{.image = resources.get_image(_mask_handle)},
  };
  recorder.begin_rendering({.color_attachments = color_attachments});
  recorder.set_viewport(_framebuffer_size);
  recorder.set_scissor(_framebuffer_size);
  recorder.bind_pipeline(_pipeline);
  recorder.set_cull_mode(graphics::Cull_mode::none);
  recorder.set_front_face(graphics::Front_face::counter_clockwise);
  recorder.bind_index_buffer(_index_buffer, graphics::Index_type::u16);
  recorder.push_data(0, std::as_bytes(std::span{&_framebuffer_size, 1}));
  recorder.draw_indexed({
    .index_count = static_cast<std::uint32_t>(_index_count),
    .instance_count = 1,
    .first_index = 0,
    .vertex_offset = 0,
    .first_instance = 0,
  });
  recorder.end_rendering();
}

} // namespace fpsparty::client::passes
