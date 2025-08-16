#include "graphics.hpp"
#include "algorithms/unordered_erase.hpp"
#include "glfw.hpp"
#include "graphics/copy_command_list.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/work_recorder.hpp"
#include "graphics/work_resource.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
namespace {
std::tuple<vk::UniqueSwapchainKHR, vk::Format, vk::Extent2D>
make_swapchain(glfw::Window window, vk::SurfaceKHR surface) {
  const auto capabilities =
      Global_vulkan_state::get().physical_device().getSurfaceCapabilitiesKHR(
          surface);
  const auto image_count =
      capabilities.maxImageCount > 0
          ? std::min(capabilities.maxImageCount, capabilities.minImageCount + 1)
          : (capabilities.minImageCount + 1);
  const auto surface_format = [&]() {
    const auto surface_formats =
        Global_vulkan_state::get().physical_device().getSurfaceFormatsKHR(
            surface);
    for (const auto &surface_format : surface_formats) {
      if (surface_format.format == vk::Format::eB8G8R8A8Srgb &&
          surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return surface_format;
      }
    }
    return surface_formats[0];
  }();
  auto const extent = [&]() {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<std::uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      const auto framebuffer_size = window.get_framebuffer_size();
      return vk::Extent2D{
          .width = std::clamp(static_cast<std::uint32_t>(framebuffer_size[0]),
                              capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width),
          .height = std::clamp(static_cast<std::uint32_t>(framebuffer_size[1]),
                               capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height)};
    }
  }();
  auto const present_mode = [&]() {
    const auto present_modes =
        Global_vulkan_state::get().physical_device().getSurfacePresentModesKHR(
            surface);
    for (const auto present_mode : present_modes) {
      if (present_mode == vk::PresentModeKHR::eMailbox) {
        return present_mode;
      }
    }
    return vk::PresentModeKHR::eFifo;
  }();
  return std::tuple{
      Global_vulkan_state::get().device().createSwapchainKHRUnique({
          .surface = surface,
          .minImageCount = image_count,
          .imageFormat = surface_format.format,
          .imageColorSpace = surface_format.colorSpace,
          .imageExtent = extent,
          .imageArrayLayers = 1,
          .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
          .imageSharingMode = vk::SharingMode::eExclusive,
          .preTransform = capabilities.currentTransform,
          .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
          .presentMode = present_mode,
          .clipped = true,
      }),
      surface_format.format, extent};
}

std::vector<vk::UniqueImageView>
make_swapchain_image_views(std::span<const vk::Image> swapchain_images,
                           vk::Format swapchain_image_format) {
  auto retval = std::vector<vk::UniqueImageView>{};
  retval.reserve(swapchain_images.size());
  for (const auto image : swapchain_images) {
    retval.emplace_back(
        Global_vulkan_state::get().device().createImageViewUnique(
            {.image = image,
             .viewType = vk::ImageViewType::e2D,
             .format = swapchain_image_format,
             .subresourceRange = {
                 .aspectMask = vk::ImageAspectFlagBits::eColor,
                 .baseMipLevel = 0,
                 .levelCount = 1,
                 .baseArrayLayer = 0,
                 .layerCount = 1,
             }}));
  }
  return retval;
}

vk::UniquePipelineLayout make_pipeline_layout() {
  auto push_constant_range = vk::PushConstantRange{};
  push_constant_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
  push_constant_range.offset = 0;
  push_constant_range.size = 64;
  return Global_vulkan_state::get().device().createPipelineLayoutUnique({
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant_range,
  });
}

class Bad_vk_shader_module_size_error : std::exception {};

vk::UniqueShaderModule load_vk_shader_module(const char *path) {
  auto input_stream = std::ifstream{};
  input_stream.exceptions(std::ios::badbit | std::ios::failbit);
  input_stream.open(path, std::ios::binary | std::ios::ate);
  const auto size = input_stream.tellg();
  if (size % sizeof(std::uint32_t) != 0) {
    throw Bad_vk_shader_module_size_error{};
  }
  auto bytecode = std::vector<std::uint32_t>{};
  bytecode.resize(size / sizeof(std::uint32_t));
  input_stream.seekg(0);
  input_stream.read(reinterpret_cast<char *>(bytecode.data()), size);
  return Global_vulkan_state::get().device().createShaderModuleUnique({
      .codeSize = static_cast<std::uint32_t>(size),
      .pCode = bytecode.data(),
  });
}

vk::UniquePipeline make_pipeline(vk::PipelineLayout pipeline_layout,
                                 vk::Format swapchain_image_format) {
  const auto vert_shader_module =
      load_vk_shader_module("./assets/shaders/shader.vert.spv");
  const auto frag_shader_module =
      load_vk_shader_module("./assets/shaders/shader.frag.spv");
  const auto shader_stages = std::vector<vk::PipelineShaderStageCreateInfo>{
      {
          .stage = vk::ShaderStageFlagBits::eVertex,
          .module = *vert_shader_module,
          .pName = "main",
      },
      {
          .stage = vk::ShaderStageFlagBits::eFragment,
          .module = *frag_shader_module,
          .pName = "main",
      },
  };
  const auto vertex_binding = vk::VertexInputBindingDescription{
      .binding = 0,
      .stride = 6 * sizeof(float),
      .inputRate = vk::VertexInputRate::eVertex,
  };
  const auto vertex_attributes =
      std::vector<vk::VertexInputAttributeDescription>{
          {
              .location = 0,
              .binding = 0,
              .format = vk::Format::eR32G32B32Sfloat,
              .offset = 0,
          },
          {
              .location = 1,
              .binding = 0,
              .format = vk::Format::eR32G32B32Sfloat,
              .offset = 3 * sizeof(float),
          },
      };
  const auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertex_binding,
      .vertexAttributeDescriptionCount =
          static_cast<std::uint32_t>(vertex_attributes.size()),
      .pVertexAttributeDescriptions = vertex_attributes.data(),
  };
  const auto input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo{
      .topology = vk::PrimitiveTopology::eTriangleList,
      .primitiveRestartEnable = vk::False,
  };
  const auto rasterization_state = vk::PipelineRasterizationStateCreateInfo{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .depthBiasEnable = vk::False,
      .lineWidth = 1.0f,
  };
  const auto blend_attachment = vk::PipelineColorBlendAttachmentState{
      .colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
  };
  const auto color_blend_state = vk::PipelineColorBlendStateCreateInfo{
      .attachmentCount = 1,
      .pAttachments = &blend_attachment,
  };
  const auto viewport_state = vk::PipelineViewportStateCreateInfo{
      .viewportCount = 1,
      .scissorCount = 1,
  };
  const auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eLess,
  };
  const auto multisample_state = vk::PipelineMultisampleStateCreateInfo{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
  };
  const auto dynamic_states = std::vector<vk::DynamicState>{
      vk::DynamicState::eViewport,          vk::DynamicState::eScissor,
      vk::DynamicState::eCullMode,          vk::DynamicState::eFrontFace,
      vk::DynamicState::ePrimitiveTopology,
  };
  const auto dynamic_state = vk::PipelineDynamicStateCreateInfo{
      .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data(),
  };
  const auto rendering_info = vk::PipelineRenderingCreateInfo{
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &swapchain_image_format,
      .depthAttachmentFormat = vk::Format::eD32Sfloat,
  };
  return std::move(Global_vulkan_state::get()
                       .device()
                       .createGraphicsPipelinesUnique(
                           {}, {vk::GraphicsPipelineCreateInfo{
                                   .pNext = &rendering_info,
                                   .stageCount = static_cast<std::uint32_t>(
                                       shader_stages.size()),
                                   .pStages = shader_stages.data(),
                                   .pVertexInputState = &vertex_input_state,
                                   .pInputAssemblyState = &input_assembly_state,
                                   .pViewportState = &viewport_state,
                                   .pRasterizationState = &rasterization_state,
                                   .pMultisampleState = &multisample_state,
                                   .pDepthStencilState = &depth_stencil_state,
                                   .pColorBlendState = &color_blend_state,
                                   .pDynamicState = &dynamic_state,
                                   .layout = pipeline_layout,
                               }})
                       .value[0]);
}

vk::UniqueSemaphore make_semaphore(const char *
#ifndef FPSPARTY_VULKAN_NDEBUG
                                       debug_name
#endif
) {
  auto retval = Global_vulkan_state::get().device().createSemaphoreUnique({});
#ifndef FPSPARTY_VULKAN_NDEBUG
  Global_vulkan_state::get().device().setDebugUtilsObjectNameEXT(
      {.objectType = vk::ObjectType::eSemaphore,
       .objectHandle = std::bit_cast<std::uint64_t>(*retval),
       .pObjectName = debug_name});
#endif
  return retval;
}
} // namespace

Graphics_buffer_copy_state::Graphics_buffer_copy_state(
    rc::Strong<Buffer> src_buffer, rc::Strong<Buffer> dst_buffer,
    vk::UniqueCommandBuffer command_buffer, vk::UniqueFence fence)
    : _src_buffer{std::move(src_buffer)},
      _dst_buffer{std::move(dst_buffer)},
      _command_buffer{std::move(command_buffer)},
      _fence{std::move(fence)} {}

void Graphics_buffer_copy_state::await() const {
  const auto lock = std::scoped_lock{_mutex};
  if (!_fence) {
    return;
  } else {
    std::ignore = Global_vulkan_state::get().device().waitForFences(
        {*_fence}, vk::False, std::numeric_limits<std::uint64_t>::max());
    _fence = {};
    _command_buffer = {};
    _dst_buffer = {};
    _src_buffer = {};
  }
}

bool Graphics_buffer_copy_state::is_done() const {
  const auto lock = std::scoped_lock{_mutex};
  if (!_fence) {
    return true;
  } else if (Global_vulkan_state::get().device().getFenceStatus(*_fence) ==
             vk::Result::eSuccess) {
    _fence = {};
    _command_buffer = {};
    _dst_buffer = {};
    _src_buffer = {};
    return true;
  } else {
    return false;
  }
}

Graphics::Graphics(const Graphics_create_info &info)
    : _window{info.window}, _surface{info.surface} {
  std::tie(_swapchain, _swapchain_image_format, _swapchain_image_extent) =
      make_swapchain(_window, _surface);
  _swapchain_images =
      Global_vulkan_state::get().device().getSwapchainImagesKHR(*_swapchain);
  _swapchain_image_views =
      make_swapchain_image_views(_swapchain_images, _swapchain_image_format);
  _pipeline_layout = make_pipeline_layout();
  _pipeline = make_pipeline(*_pipeline_layout, _swapchain_image_format);
  _copy_command_pool =
      Global_vulkan_state::get().device().createCommandPoolUnique({
          .flags = vk::CommandPoolCreateFlagBits::eTransient,
          .queueFamilyIndex = Global_vulkan_state::get().queue_family_index(),
      });
  for (auto i = std::size_t{}; i != info.max_frames_in_flight; ++i) {
    const auto swapchain_image_acquire_semaphore_name =
        "swapchain_image_acquire_semaphores[" + std::to_string(i) + "]";
    const auto swapchain_image_release_semaphore_name =
        "swapchain_image_release_semaphores[" + std::to_string(i) + "]";
    _frame_resources.push_back({
        .swapchain_image_acquire_semaphore =
            make_semaphore("swapchain_image_acquire_semaphore"),
        .swapchain_image_release_semaphore =
            make_semaphore("swapchain_image_release_semaphore"),
        .work_done_fence =
            Global_vulkan_state::get().device().createFenceUnique({
                .flags = vk::FenceCreateFlagBits::eSignaled,
            }),
        .command_pool =
            Global_vulkan_state::get().device().createCommandPoolUnique({
                .flags = vk::CommandPoolCreateFlagBits::eTransient,
                .queueFamilyIndex =
                    Global_vulkan_state::get().queue_family_index(),
            }),
    });
    _frame_resources.back().command_buffer = std::move(
        Global_vulkan_state::get().device().allocateCommandBuffersUnique({
            .commandPool = *_frame_resources.back().command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        })[0]);
    std::tie(_frame_resources.back().depth_image,
             _frame_resources.back().depth_allocation) =
        Global_vulkan_state::get().allocator().create_image_unique(
            {
                .imageType = vk::ImageType::e2D,
                .format = vk::Format::eD32Sfloat,
                .extent =
                    vk::Extent3D{
                        _swapchain_image_extent.width,
                        _swapchain_image_extent.height,
                        1,
                    },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                .sharingMode = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined,
            },
            {
                .flags = {},
                .usage = c_repr(vma::Memory_usage::e_auto),
                .requiredFlags = {},
                .preferredFlags = {},
                .memoryTypeBits = {},
                .pool = {},
                .pUserData = {},
                .priority = {},
            });
    _frame_resources.back().depth_image_view =
        Global_vulkan_state::get().device().createImageViewUnique({
            .image = *_frame_resources.back().depth_image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eD32Sfloat,
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eDepth,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        });
  }
}

void Graphics::poll_works() {
  for (const auto &work : _pending_works) {
    if (detail::poll_work(*work)) {
      auto resource = detail::release_work(*work);
      detail::reset_work_resource(resource);
      _work_resource_pool.push(std::move(resource));
    }
  }
  algorithms::unordered_erase_many_if(
      _pending_works,
      [&](const rc::Strong<Work> &work) { return work->is_done(); });
}

rc::Strong<Pipeline_layout>
Graphics::create_pipeline_layout(const Pipeline_layout_create_info &info) {
  return _pipeline_layout_factory.create(info);
}

rc::Strong<Pipeline>
Graphics::create_pipeline(const Pipeline_create_info &info) {
  return _pipeline_factory.create(info);
}

rc::Strong<Staging_buffer>
Graphics::create_staging_buffer(std::span<const std::byte> data) {
  return _staging_buffer_factory.create(data);
}

rc::Strong<Vertex_buffer> Graphics::create_vertex_buffer(std::size_t size) {
  return _vertex_buffer_factory.create(size);
}

rc::Strong<Index_buffer> Graphics::create_index_buffer(std::size_t size) {
  return _index_buffer_factory.create(size);
}

/*
std::pair<rc::Strong<Vertex_buffer>, rc::Strong<Graphics_buffer_copy_state>>
Graphics::create_vertex_buffer(std::span<const std::byte> data) {
  auto staging_buffer = _staging_buffer_factory.create(data);
  auto vertex_buffer = _vertex_buffer_factory.create(data.size());
  auto upload_command_buffer = make_upload_command_buffer();
  upload_command_buffer->begin({
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  });
  upload_command_buffer->copyBuffer(
      staging_buffer->get_buffer(), vertex_buffer->get_buffer(),
      {{
          .srcOffset = 0,
          .dstOffset = 0,
          .size = static_cast<vk::DeviceSize>(data.size()),
      }});
  upload_command_buffer->end();
  auto upload_fence = Global_vulkan_state::get().device().createFenceUnique({});
  Global_vulkan_state::get().queue().submit(
      {{
          .commandBufferCount = 1,
          .pCommandBuffers = &*upload_command_buffer,
      }},
      *upload_fence);
  auto upload_state = _buffer_copy_state_factory.create(
      std::move(staging_buffer), vertex_buffer,
      std::move(upload_command_buffer), std::move(upload_fence));
  _buffer_copy_states.emplace_back(upload_state);
  return std::pair{std::move(vertex_buffer), std::move(upload_state)};
}

std::pair<rc::Strong<Index_buffer>, rc::Strong<Graphics_buffer_copy_state>>
Graphics::create_index_buffer(std::span<const std::byte> data) {
  auto staging_buffer = _staging_buffer_factory.create(data);
  auto index_buffer = _index_buffer_factory.create(data.size());
  auto upload_command_buffer = make_upload_command_buffer();
  upload_command_buffer->begin({
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  });
  upload_command_buffer->copyBuffer(
      staging_buffer->get_buffer(), index_buffer->get_buffer(),
      {{
          .srcOffset = 0,
          .dstOffset = 0,
          .size = static_cast<vk::DeviceSize>(data.size()),
      }});
  upload_command_buffer->end();
  auto upload_fence = Global_vulkan_state::get().device().createFenceUnique({});
  Global_vulkan_state::get().queue().submit(
      {{
          .commandBufferCount = 1,
          .pCommandBuffers = &*upload_command_buffer,
      }},
      *upload_fence);
  auto upload_state = _buffer_copy_state_factory.create(
      std::move(staging_buffer), index_buffer, std::move(upload_command_buffer),
      std::move(upload_fence));
  _buffer_copy_states.emplace_back(upload_state);
  return std::pair{std::move(index_buffer), std::move(upload_state)};
}
*/

Work_recorder Graphics::begin_transient_work() {
  return detail::acquire_work_recorder(_work_resource_pool.pop());
}

bool Graphics::begin() {
  auto &frame_resource = _frame_resources[_frame_resource_index];
  try {
    const auto swapchain_image_index =
        Global_vulkan_state::get().device().acquireNextImageKHR(
            *_swapchain, std::numeric_limits<std::uint64_t>::max(),
            *frame_resource.swapchain_image_acquire_semaphore);
    frame_resource.swapchain_image_index = swapchain_image_index.value;
  } catch (const vk::OutOfDateKHRError &e) {
    remake_swapchain();
    return false;
  }
  const auto swapchain_image =
      _swapchain_images[frame_resource.swapchain_image_index];
  const auto swapchain_image_view =
      *_swapchain_image_views[frame_resource.swapchain_image_index];
  std::ignore = Global_vulkan_state::get().device().waitForFences(
      1, &*frame_resource.work_done_fence, true,
      std::numeric_limits<std::uint64_t>::max());
  std::ignore = Global_vulkan_state::get().device().resetFences(
      1, &*frame_resource.work_done_fence);
  Global_vulkan_state::get().device().resetCommandPool(
      *frame_resource.command_pool);
  frame_resource.used_buffers.clear();
  frame_resource.command_buffer->begin(vk::CommandBufferBeginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  });
  const auto swapchain_image_barrier_1 = vk::ImageMemoryBarrier2{
      .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
      .srcAccessMask = {},
      .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
      .oldLayout = vk::ImageLayout::eUndefined,
      .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = swapchain_image,
      .subresourceRange =
          {
              .aspectMask = vk::ImageAspectFlagBits::eColor,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };
  frame_resource.command_buffer->pipelineBarrier2({
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &swapchain_image_barrier_1,
  });
  const auto color_attachment = vk::RenderingAttachmentInfo{
      .imageView = swapchain_image_view,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = {{0.4196f, 0.6196f, 0.7451f, 1.0f}},
  };
  const auto depth_attachment = vk::RenderingAttachmentInfo{
      .imageView = *frame_resource.depth_image_view,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = {vk::ClearDepthStencilValue{.depth = 1.0f}},
  };
  frame_resource.command_buffer->beginRendering({
      .renderArea = {.offset = {0, 0}, .extent = _swapchain_image_extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment,
      .pDepthAttachment = &depth_attachment,
  });
  frame_resource.command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                              *_pipeline);
  frame_resource.command_buffer->setViewport(
      0, {{
             .width = static_cast<float>(_swapchain_image_extent.width),
             .height = static_cast<float>(_swapchain_image_extent.height),
             .minDepth = 0.0f,
             .maxDepth = 1.0f,
         }});
  frame_resource.command_buffer->setScissor(
      0, {{.extent = _swapchain_image_extent}});
  frame_resource.command_buffer->setCullMode(vk::CullModeFlagBits::eBack);
  frame_resource.command_buffer->setFrontFace(vk::FrontFace::eCounterClockwise);
  frame_resource.command_buffer->setPrimitiveTopology(
      vk::PrimitiveTopology::eTriangleList);
  return true;
}

void Graphics::end() {
  const auto &frame_resource = _frame_resources[_frame_resource_index];
  const auto swapchain_image =
      _swapchain_images[frame_resource.swapchain_image_index];
  frame_resource.command_buffer->endRendering();
  const auto swapchain_image_barrier_2 = vk::ImageMemoryBarrier2{
      .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
      .dstAccessMask = {},
      .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .newLayout = vk::ImageLayout::ePresentSrcKHR,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = swapchain_image,
      .subresourceRange =
          {
              .aspectMask = vk::ImageAspectFlagBits::eColor,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };
  frame_resource.command_buffer->pipelineBarrier2({
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &swapchain_image_barrier_2,
  });
  frame_resource.command_buffer->end();
  const auto vk_image_acquire_wait_stage =
      vk::PipelineStageFlags{vk::PipelineStageFlagBits::eTopOfPipe};
  Global_vulkan_state::get().queue().submit(
      {{
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &*frame_resource.swapchain_image_acquire_semaphore,
          .pWaitDstStageMask = &vk_image_acquire_wait_stage,
          .commandBufferCount = 1,
          .pCommandBuffers = &*frame_resource.command_buffer,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores =
              &*frame_resource.swapchain_image_release_semaphore,
      }},
      *frame_resource.work_done_fence);
  try {
    const auto vk_queue_present_result =
        Global_vulkan_state::get().queue().presentKHR({
            .waitSemaphoreCount = 1,
            .pWaitSemaphores =
                &*frame_resource.swapchain_image_release_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &*_swapchain,
            .pImageIndices = &frame_resource.swapchain_image_index,
        });
    if (vk_queue_present_result == vk::Result::eSuboptimalKHR) {
      throw vk::OutOfDateKHRError{"Subobtimal queue present result"};
    }
  } catch (const vk::OutOfDateKHRError &e) {
    remake_swapchain();
  }
  _frame_resource_index = (_frame_resource_index + 1) % _frame_resources.size();
}

void Graphics::bind_vertex_buffer(rc::Strong<const Vertex_buffer> buffer) {
  auto &frame_resource = _frame_resources[_frame_resource_index];
  frame_resource.command_buffer->bindVertexBuffers(0, {buffer->get_buffer()},
                                                   {0});
  frame_resource.used_buffers.emplace_back(std::move(buffer));
}

void Graphics::bind_index_buffer(rc::Strong<const Index_buffer> buffer,
                                 vk::IndexType index_type) {
  auto &frame_resource = _frame_resources[_frame_resource_index];
  frame_resource.command_buffer->bindIndexBuffer(buffer->get_buffer(), 0,
                                                 index_type);
  frame_resource.used_buffers.emplace_back(std::move(buffer));
}

void Graphics::draw_indexed(std::uint32_t index_count) noexcept {
  _frame_resources[_frame_resource_index].command_buffer->drawIndexed(
      index_count, 1, 0, 0, 0);
}

void Graphics::push_constants(
    const Eigen::Matrix4f &model_view_projection_matrix) noexcept {
  _frame_resources[_frame_resource_index].command_buffer->pushConstants(
      *_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, 64,
      model_view_projection_matrix.data());
}

vk::UniqueCommandBuffer Graphics::make_upload_command_buffer() {
  return std::move(
      Global_vulkan_state::get().device().allocateCommandBuffersUnique({
          .commandPool = *_copy_command_pool,
          .level = vk::CommandBufferLevel::ePrimary,
          .commandBufferCount = 1,
      })[0]);
}

void Graphics::remake_swapchain() {
  Global_vulkan_state::get().device().waitIdle();
  _swapchain_image_views.clear();
  _swapchain_images.clear();
  _swapchain.reset();
  const auto previous_vk_swapchain_image_format = _swapchain_image_format;
  std::tie(_swapchain, _swapchain_image_format, _swapchain_image_extent) =
      make_swapchain(_window, _surface);
  _swapchain_images =
      Global_vulkan_state::get().device().getSwapchainImagesKHR(*_swapchain);
  _swapchain_image_views =
      make_swapchain_image_views(_swapchain_images, _swapchain_image_format);
  if (_swapchain_image_format != previous_vk_swapchain_image_format) {
    std::cout << "Swapchain changed image format.\n";
    _pipeline = make_pipeline(*_pipeline_layout, _swapchain_image_format);
  }
}
} // namespace fpsparty::graphics
