#include "work_recorder.hpp"
#include "graphics/buffer.hpp"

namespace fpsparty::graphics {
namespace detail {
Work_recorder acquire_work_recorder(Work_resource resource) {
  return Work_recorder{std::move(resource)};
}

Work_resource release_work_recorder(Work_recorder recorder) {
  return std::move(recorder._resource);
}
} // namespace detail

void Work_recorder::copy_buffer(rc::Strong<const Buffer> src,
                                rc::Strong<Buffer> dst,
                                const Buffer_copy_info &info) {
  get_command_buffer().copyBuffer(
      detail::get_buffer_vk_buffer(*src), detail::get_buffer_vk_buffer(*dst),
      vk::BufferCopy{
          .srcOffset = static_cast<vk::DeviceSize>(info.src_offset),
          .dstOffset = static_cast<vk::DeviceSize>(info.dst_offset),
          .size = static_cast<vk::DeviceSize>(info.size),
      });
  add_reference(std::move(src));
  add_reference(std::move(dst));
}

void Work_recorder::bind_vertex_buffer(rc::Strong<const Vertex_buffer> buffer) {
  get_command_buffer().bindVertexBuffers(
      0, {detail::get_buffer_vk_buffer(*buffer)}, {0});
  add_reference(std::move(buffer));
}

void Work_recorder::bind_index_buffer(rc::Strong<const Index_buffer> buffer,
                                      Index_type index_type) {
  get_command_buffer().bindIndexBuffer(detail::get_buffer_vk_buffer(*buffer), 0,
                                       static_cast<vk::IndexType>(index_type));
  add_reference(std::move(buffer));
}

void Work_recorder::draw_indexed(std::uint32_t index_count) noexcept {
  get_command_buffer().drawIndexed(index_count, 1, 0, 0, 0);
}

void Work_recorder::push_constants(
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
