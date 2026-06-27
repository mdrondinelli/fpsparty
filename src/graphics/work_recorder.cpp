#include "work_recorder.hpp"

#include <cassert>

#include "buffer.hpp"
#include "global_vulkan_state.hpp"
#include "image.hpp"

namespace fpsparty::graphics {

namespace detail {

namespace {

void bind_sampler_heap(
  Work_resource &resource, rc::Strong<Buffer> sampler_heap) {
  resource.vk_command_buffer.bindSamplerHeapEXT({
    .heapRange =
      {
        .address = sampler_heap->get_device_address(),
        .size = sampler_heap->get_size(),
      },
    .reservedRangeOffset =
      sampler_heap->get_size() - Global_vulkan_state::get()
                                   .descriptor_heap_properties()
                                   .minSamplerHeapReservedRange,
    .reservedRangeSize = Global_vulkan_state::get()
                           .descriptor_heap_properties()
                           .minSamplerHeapReservedRange,
  });
  resource.buffers.emplace_back(std::move(sampler_heap));
}

void bind_resource_heap(
  Work_resource &resource, rc::Strong<Buffer> resource_heap) {
  resource.vk_command_buffer.bindResourceHeapEXT({
    .heapRange =
      {
        .address = resource_heap->get_device_address(),
        .size = resource_heap->get_size(),
      },
    .reservedRangeOffset =
      resource_heap->get_size() - Global_vulkan_state::get()
                                    .descriptor_heap_properties()
                                    .minResourceHeapReservedRange,
    .reservedRangeSize = Global_vulkan_state::get()
                           .descriptor_heap_properties()
                           .minResourceHeapReservedRange,
  });
  resource.buffers.emplace_back(std::move(resource_heap));
}

} // namespace

Work_recorder acquire_work_recorder(
  Work_resource resource,
  std::optional<Work_recorder_descriptor_info> const
    &descriptor_info) noexcept {
  resource.vk_command_buffer.begin({
    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  });
  if (descriptor_info) {
    resource.descriptor_allocation = descriptor_info->descriptor_allocation;
    bind_sampler_heap(resource, descriptor_info->sampler_heap);
    bind_resource_heap(resource, descriptor_info->resource_heap);
  }
  auto recorder = Work_recorder{std::move(resource)};
  recorder._descriptor_data =
    descriptor_info ? descriptor_info->descriptor_data : nullptr;
  return recorder;
}

Work_resource release_work_recorder(Work_recorder recorder) noexcept {
  recorder._resource.vk_command_buffer.end();
  return std::move(recorder._resource);
}

} // namespace detail

std::uint32_t
Work_recorder::upload_sampled_image_descriptor(rc::Strong<Image const> image) {
  auto const descriptor = image->get_sampled_image_descriptor();
  auto const index = alloc_descriptor();
  std::memcpy(
    _descriptor_data + index * descriptor.size(),
    descriptor.data(),
    descriptor.size());
  add_reference(std::move(image));
  return index;
}

std::uint32_t
Work_recorder::upload_storage_image_descriptor(rc::Strong<Image> image) {
  auto const descriptor = image->get_storage_image_descriptor();
  auto const index = alloc_descriptor();
  std::memcpy(
    _descriptor_data + index * descriptor.size(),
    descriptor.data(),
    descriptor.size());
  add_reference(std::move(image));
  return index;
}

std::uint32_t Work_recorder::alloc_descriptor() {
  assert(_descriptor_data);
  assert(_resource.descriptor_allocation);
  auto const &allocation = *_resource.descriptor_allocation;
  if (_descriptor_count >= allocation.size) {
    throw std::runtime_error{"Descriptor heap allocation is too small"};
  }
  return allocation.offset + _descriptor_count++;
}

void Work_recorder::copy_buffer(
  rc::Strong<Buffer const> src,
  rc::Strong<Buffer> dst,
  Buffer_copy_info const &info) {
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

void Work_recorder::copy_buffer_to_image(
  rc::Strong<Buffer const> src,
  rc::Strong<Image> dst,
  Buffer_image_copy_info const &info) {
  get_command_buffer().copyBufferToImage(
    detail::get_buffer_vk_buffer(*src),
    detail::get_image_vk_image(*dst),
    vk::ImageLayout::eGeneral,
    vk::BufferImageCopy{
      .bufferOffset = static_cast<vk::DeviceSize>(info.src_offset),
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource =
        {
          .aspectMask =
            detail::get_image_format_vk_image_aspect_flags(dst->get_format()),
          .mipLevel = info.dst_mip_level,
          .baseArrayLayer = info.dst_base_array_layer,
          .layerCount = info.dst_array_layer_count,
        },
      .imageOffset =
        vk::Offset3D{
          info.dst_offset.x(), info.dst_offset.y(), info.dst_offset.z()},
      .imageExtent =
        vk::Extent3D{
          static_cast<std::uint32_t>(info.dst_extent.x()),
          static_cast<std::uint32_t>(info.dst_extent.y()),
          static_cast<std::uint32_t>(info.dst_extent.z())},
    });
  add_reference(std::move(src));
  add_reference(std::move(dst));
}

void Work_recorder::barrier(
  Synchronization_scope const &src_scope,
  Synchronization_scope const &dst_scope) {
  auto const barrier = vk::MemoryBarrier2{
    .srcStageMask = static_cast<vk::PipelineStageFlags2>(src_scope.stage_mask),
    .srcAccessMask = static_cast<vk::AccessFlags2>(src_scope.access_mask),
    .dstStageMask = static_cast<vk::PipelineStageFlags2>(dst_scope.stage_mask),
    .dstAccessMask = static_cast<vk::AccessFlags2>(dst_scope.access_mask),
  };
  get_command_buffer().pipelineBarrier2({
    .memoryBarrierCount = 1,
    .pMemoryBarriers = &barrier,
  });
}

void Work_recorder::transition_image_layout(
  Synchronization_scope const &src_scope,
  Synchronization_scope const &dst_scope,
  Image_layout old_layout,
  Image_layout new_layout,
  rc::Strong<Image> image) {
  auto const barrier = vk::ImageMemoryBarrier2{
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
        .aspectMask =
          detail::get_image_format_vk_image_aspect_flags(image->get_format()),
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

void Work_recorder::begin_rendering(Rendering_begin_info const &info) {
  auto const color_attachment = vk::RenderingAttachmentInfo{
    .imageView = detail::get_image_vk_image_view(*info.color_image),
    .imageLayout = vk::ImageLayout::eGeneral,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = {{
      info.color_clear_value.x(),
      info.color_clear_value.y(),
      info.color_clear_value.z(),
      info.color_clear_value.w(),
    }},
  };
  auto const depth_attachment = vk::RenderingAttachmentInfo{
    .imageView = info.depth_image
                   ? detail::get_image_vk_image_view(*info.depth_image)
                   : vk::ImageView{},
    .imageLayout = vk::ImageLayout::eGeneral,
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

void Work_recorder::bind_pipeline(rc::Strong<Pipeline const> pipeline) {
  get_command_buffer().bindPipeline(
    vk::PipelineBindPoint::eGraphics,
    detail::get_pipeline_vk_pipeline(*pipeline));
  add_reference(std::move(pipeline));
}

void Work_recorder::bind_compute_pipeline(
  rc::Strong<Compute_pipeline const> pipeline) {
  get_command_buffer().bindPipeline(
    vk::PipelineBindPoint::eCompute,
    detail::get_compute_pipeline_vk_pipeline(*pipeline));
  add_reference(std::move(pipeline));
}

void Work_recorder::set_cull_mode(Cull_mode cull_mode) {
  get_command_buffer()
    .setCullMode(static_cast<vk::CullModeFlags>(static_cast<int>(cull_mode)));
}

void Work_recorder::set_front_face(Front_face front_face) {
  get_command_buffer().setFrontFace(static_cast<vk::FrontFace>(front_face));
}

void Work_recorder::set_viewport(Eigen::Vector2i const &extent) {
  get_command_buffer().setViewport(
    0,
    {{
      .width = static_cast<float>(extent.x()),
      .height = static_cast<float>(extent.y()),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    }});
}

void Work_recorder::set_scissor(Eigen::Vector2i const &extent) {
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

void Work_recorder::bind_index_buffer(
  rc::Strong<Buffer const> buffer, Index_type index_type) {
  get_command_buffer().bindIndexBuffer(
    detail::get_buffer_vk_buffer(*buffer),
    0,
    static_cast<vk::IndexType>(index_type));
  add_reference(std::move(buffer));
}

void Work_recorder::draw_indexed(Indexed_draw_info const &info) noexcept {
  get_command_buffer().drawIndexed(
    info.index_count,
    info.instance_count,
    info.first_index,
    info.vertex_offset,
    info.first_instance);
}

void Work_recorder::draw_indexed_indirect(
  Indirect_indexed_draw_info const &info) noexcept {
  get_command_buffer().drawIndexedIndirect(
    detail::get_buffer_vk_buffer(*info.buffer),
    info.offset,
    info.draw_count,
    info.stride);
  add_reference(info.buffer);
}

void Work_recorder::dispatch(
  std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
  get_command_buffer().dispatch(x, y, z);
}

void Work_recorder::push_data(
  std::uint32_t offset, std::span<std::byte const> data) noexcept {
  get_command_buffer().pushDataEXT({
    .offset = offset,
    .data =
      {
        .address = data.data(),
        .size = data.size(),
      },
  });
}

void Work_recorder::push_buffer(
  std::uint32_t offset, rc::Strong<Buffer> buffer) noexcept {
  auto const address = buffer->get_device_address();
  push_data(offset, std::as_bytes(std::span{&address, 1}));
  add_reference(std::move(buffer));
}

Work_recorder::Work_recorder(detail::Work_resource resource) noexcept
    : _resource{std::move(resource)} {}

void Work_recorder::add_reference(rc::Strong<Buffer const> buffer) {
  _resource.buffers.emplace_back(std::move(buffer));
}

void Work_recorder::add_reference(rc::Strong<Image const> image) {
  _resource.images.emplace_back(std::move(image));
}

void Work_recorder::add_reference(rc::Strong<Pipeline const> pipeline) {
  _resource.pipelines.emplace_back(std::move(pipeline));
}

void Work_recorder::add_reference(rc::Strong<Compute_pipeline const> pipeline) {
  _resource.compute_pipelines.emplace_back(std::move(pipeline));
}

vk::CommandBuffer Work_recorder::get_command_buffer() const noexcept {
  return _resource.vk_command_buffer;
}

} // namespace fpsparty::graphics
