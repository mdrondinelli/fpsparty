#include "client/passes/composite_pass.hpp"
#include "render_graph/access.hpp"
#include <array>
#include <span>
#include <utility>

namespace fpsparty::client::passes {

Composite_pass::Composite_pass(std::size_t index_count)
    : _index_count{index_count} {}

void Composite_pass::update(Composite_pass_inputs inputs) {
  _inputs = std::move(inputs);
}

void Composite_pass::declare(render_graph::Builder &builder) {
  _swapchain_handle = builder.write(
    _inputs.swapchain_image, render_graph::access::color_attachment_write);
  _radiance_handle = builder.read(
    _inputs.radiance_descriptor, render_graph::access::fragment_sampled_read);
  _crosshair_mask_handle = builder.read(
    _inputs.crosshair_mask_descriptor,
    render_graph::access::fragment_sampled_read);
}

void Composite_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &) {
  // Freshly acquired each frame (unlike every other render target here,
  // which transitions once at creation and stays general forever) -- so
  // it needs its own transition, not just a memory barrier.
  recorder.transition_image_layout(
    {},
    render_graph::access::color_attachment_write,
    graphics::Image_layout::undefined,
    graphics::Image_layout::general,
    _inputs.swapchain_image);
  auto const color_attachments = std::array{
    graphics::Color_attachment_info{.image = _inputs.swapchain_image},
  };
  recorder.begin_rendering({.color_attachments = color_attachments});
  recorder.set_viewport(_inputs.framebuffer_size);
  recorder.set_scissor(_inputs.framebuffer_size);
  recorder.bind_pipeline(_inputs.pipeline);
  recorder.set_cull_mode(graphics::Cull_mode::none);
  recorder.set_front_face(graphics::Front_face::counter_clockwise);
  recorder.bind_index_buffer(_inputs.index_buffer, graphics::Index_type::u16);
  recorder.push_descriptors(
    0, {_inputs.radiance_descriptor, _inputs.crosshair_mask_descriptor});
  recorder.push_data(4, std::as_bytes(std::span{&_inputs.frame_number, 1}));
  recorder.draw_indexed({
    .index_count = static_cast<u32>(_index_count),
    .instance_count = 1,
    .first_index = 0,
    .vertex_offset = 0,
    .first_instance = 0,
  });
  recorder.end_rendering();
}

} // namespace fpsparty::client::passes
