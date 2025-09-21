#include "work_recorder.hpp"
#include "graphics/buffer.hpp"
#include "graphics/image.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace fpsparty::graphics {
namespace detail {
Work_recorder acquire_work_recorder(Work_resource resource) noexcept {
  resource.vk_command_buffer.begin({
    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  });
  return Work_recorder{std::move(resource)};
}

Work_resource release_work_recorder(Work_recorder recorder) noexcept {
  recorder._resource.vk_command_buffer.end();
  return std::move(recorder._resource);
}
} // namespace detail

void Work_recorder::copy_buffer(
  rc::Strong<const Buffer> src,
  rc::Strong<Buffer> dst,
  const Buffer_copy_info &info) {
  get_command_buffer().copyBuffer(
    detail::get_buffer_vk_buffer(*src),
    detail::get_buffer_vk_buffer(*dst),
    vk::BufferCopy{
      .srcOffset = static_cast<vk::DeviceSize>(info.src_offset),
      .dstOffset = static_cast<vk::DeviceSize>(info.dst_offset),
      .size = static_cast<vk::DeviceSize>(info.size),
    });
  add_reference(std::move(src));
  add_reference(std::move(dst));
}

void Work_recorder::transition_image_layout(
  const Synchronization_scope &src_scope,
  const Synchronization_scope &dst_scope,
  Image_layout old_layout,
  Image_layout new_layout,
  rc::Strong<Image> image) {
  const auto barrier = vk::ImageMemoryBarrier2{
    .srcStageMask = static_cast<vk::PipelineStageFlags2>(src_scope.stage_mask),
    .srcAccessMask = static_cast<vk::AccessFlags2>(src_scope.access_mask),
    .dstStageMask = static_cast<vk::PipelineStageFlags2>(dst_scope.stage_mask),
    .dstAccessMask = static_cast<vk::AccessFlags2>(dst_scope.access_mask),
    .oldLayout = static_cast<vk::ImageLayout>(old_layout),
    .newLayout = static_cast<vk::ImageLayout>(new_layout),
    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
    .image = detail::get_image_vk_image(*image),
    .subresourceRange =
      {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = static_cast<std::uint32_t>(image->get_mip_level_count()),
        .baseArrayLayer = 0,
        .layerCount =
          static_cast<std::uint32_t>(image->get_array_layer_count()),
      },
  };
  get_command_buffer().pipelineBarrier2({
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrier,
  });
  add_reference(std::move(image));
}

void Work_recorder::begin_rendering(const Rendering_begin_info &info) {
  const auto color_attachment = vk::RenderingAttachmentInfo{
    .imageView = detail::get_image_vk_image_view(*info.color_image),
    .imageLayout = vk::ImageLayout::eGeneral,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = {{0.4196f, 0.6196f, 0.7451f, 1.0f}},
  };
  const auto depth_attachment = vk::RenderingAttachmentInfo{
    .imageView = info.depth_image
                   ? detail::get_image_vk_image_view(*info.depth_image)
                   : vk::ImageView{},
    .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = {vk::ClearDepthStencilValue{.depth = 0.0f}},
  };
  get_command_buffer().beginRendering({
    .renderArea =
      {.offset = {0, 0},
       .extent =
         {
           static_cast<std::uint32_t>(info.color_image->get_extent().x()),
           static_cast<std::uint32_t>(info.color_image->get_extent().y()),
         }},
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
    .pDepthAttachment = info.depth_image ? &depth_attachment : nullptr,
  });
  add_reference(info.color_image);
  if (info.depth_image) {
    add_reference(info.depth_image);
  }
}

void Work_recorder::end_rendering() { get_command_buffer().endRendering(); }

void Work_recorder::bind_pipeline(rc::Strong<const Pipeline> pipeline) {
  get_command_buffer().bindPipeline(
    vk::PipelineBindPoint::eGraphics,
    detail::get_pipeline_vk_pipeline(*pipeline));
  add_reference(std::move(pipeline));
}

void Work_recorder::set_cull_mode(Cull_mode cull_mode) {
  get_command_buffer()
    .setCullMode(static_cast<vk::CullModeFlags>(static_cast<int>(cull_mode)));
}

void Work_recorder::set_front_face(Front_face front_face) {
  get_command_buffer().setFrontFace(static_cast<vk::FrontFace>(front_face));
}

void Work_recorder::set_viewport(const Eigen::Vector2i &extent) {
  get_command_buffer().setViewport(
    0,
    {{
      .width = static_cast<float>(extent.x()),
      .height = static_cast<float>(extent.y()),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    }});
}

void Work_recorder::set_scissor(const Eigen::Vector2i &extent) {
  get_command_buffer().setScissor(
    0,
    {{
      .extent =
        {
          static_cast<std::uint32_t>(extent.x()),
          static_cast<std::uint32_t>(extent.y()),
        },
    }});
}

void Work_recorder::set_depth_test_enabled(bool enabled) {
  get_command_buffer().setDepthTestEnable(enabled ? vk::True : vk::False);
}

void Work_recorder::set_depth_write_enabled(bool enabled) {
  get_command_buffer().setDepthWriteEnable(enabled ? vk::True : vk::False);
}

void Work_recorder::set_depth_compare_op(Compare_op op) {
  get_command_buffer().setDepthCompareOp(static_cast<vk::CompareOp>(op));
}

void Work_recorder::bind_vertex_buffer(rc::Strong<const Vertex_buffer> buffer) {
  get_command_buffer()
    .bindVertexBuffers(0, {detail::get_buffer_vk_buffer(*buffer)}, {0});
  add_reference(std::move(buffer));
}

void Work_recorder::bind_index_buffer(
  rc::Strong<const Index_buffer> buffer, Index_type index_type) {
  get_command_buffer().bindIndexBuffer(
    detail::get_buffer_vk_buffer(*buffer),
    0,
    static_cast<vk::IndexType>(index_type));
  add_reference(std::move(buffer));
}

void Work_recorder::draw_indexed(const Indexed_draw_info &info) noexcept {
  get_command_buffer().drawIndexed(
    info.index_count,
    info.instance_count,
    info.first_index,
    info.vertex_offset,
    info.first_instance);
}

void Work_recorder::push_constants(
  rc::Strong<const Pipeline_layout> pipeline_layout,
  Shader_stage_flags stage_flags,
  std::uint32_t offset,
  std::span<const std::byte> data) noexcept {
  get_command_buffer().pushConstants(
    detail::get_pipeline_layout_vk_pipeline_layout(*pipeline_layout),
    static_cast<vk::ShaderStageFlags>(stage_flags),
    offset,
    static_cast<std::uint32_t>(data.size()),
    data.data());
  add_reference(std::move(pipeline_layout));
}

Work_recorder::Work_recorder(detail::Work_resource resource) noexcept
    : _resource{std::move(resource)} {}

void Work_recorder::add_reference(rc::Strong<const Buffer> buffer) {
  _resource.buffers.emplace_back(std::move(buffer));
}

void Work_recorder::add_reference(rc::Strong<const Image> image) {
  _resource.images.emplace_back(std::move(image));
}

void Work_recorder::add_reference(
  rc::Strong<const Pipeline_layout> pipeline_layout) {
  _resource.pipeline_layouts.emplace_back(std::move(pipeline_layout));
}

void Work_recorder::add_reference(rc::Strong<const Pipeline> pipeline) {
  _resource.pipelines.emplace_back(std::move(pipeline));
}

vk::CommandBuffer Work_recorder::get_command_buffer() const noexcept {
  return _resource.vk_command_buffer;
}
} // namespace fpsparty::graphics
