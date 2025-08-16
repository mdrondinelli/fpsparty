#include "render_command_list.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
Render_command_list::Render_command_list(
    vk::UniqueCommandBuffer command_buffer) noexcept
    : Command_list{std::move(command_buffer)} {}

void Render_command_list::bind_vertex_buffer(
    rc::Strong<const Vertex_buffer> buffer) {
  get_command_buffer().bindVertexBuffers(0, {buffer->get_buffer()}, {0});
  add_reference(std::move(buffer));
}

void Render_command_list::bind_index_buffer(
    rc::Strong<const Index_buffer> buffer, Index_type index_type) {
  get_command_buffer().bindIndexBuffer(buffer->get_buffer(), 0,
                                       static_cast<vk::IndexType>(index_type));
  add_reference(std::move(buffer));
}

void Render_command_list::draw_indexed(std::uint32_t index_count) noexcept {
  get_command_buffer().drawIndexed(index_count, 1, 0, 0, 0);
}

void Render_command_list::push_constants(
    rc::Strong<const Pipeline_layout> pipeline_layout,
    Shader_stage_flags stage_flags, std::uint32_t offset,
    std::span<const std::byte> data) noexcept {
  get_command_buffer().pushConstants(
      pipeline_layout->get_vk_pipeline_layout(),
      static_cast<vk::ShaderStageFlags>(stage_flags), offset,
      static_cast<std::uint32_t>(data.size()), data.data());
  add_reference(std::move(pipeline_layout));
}
} // namespace fpsparty::graphics
