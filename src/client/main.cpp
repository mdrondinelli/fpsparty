#include "client/global_vulkan_state.hpp"
#include "client/index_buffer.hpp"
#include "client/transformation_matrices.hpp"
#include "client/vertex_buffer.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "game/replicated_game.hpp"
#include "glfw.hpp"
#include "net/client.hpp"
#include "vma.hpp"
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <tuple>
#include <volk.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_funcs.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

using namespace fpsparty;
using namespace fpsparty::client;

namespace vk {
DispatchLoaderDynamic defaultDispatchLoaderDynamic;
}

namespace {
constexpr auto server_ip = "127.0.0.1";

volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }

vk::UniqueSurfaceKHR make_vk_surface(glfw::Window window) {
  auto retval = glfw::create_window_surface_unique(
      Global_vulkan_state::get().instance(), window);
  std::cout << "Created VkSurfaceKHR.\n";
  return retval;
}

class Bad_vk_shader_module_size_error : std::exception {};

struct Vertex {
  float x;
  float y;
  float z;
  float r;
  float g;
  float b;
};

const auto floor_mesh_vertices = std::vector<Vertex>{
    {.x = 10.0f, .y = -0.5f, .z = 10.0f, .r = 1.0f, .g = 0.0f, .b = 0.0f},
    {.x = 10.0f, .y = -0.5f, .z = -10.0f, .r = 0.0f, .g = 0.0f, .b = 1.0f},
    {.x = -10.0f, .y = -0.5f, .z = 10.0f, .r = 0.0f, .g = 1.0f, .b = 0.0f},
    {.x = -10.0f, .y = -0.5f, .z = -10.0f, .r = 1.0f, .g = 1.0f, .b = 0.0f},
};

const auto floor_mesh_indices = std::vector<std::uint16_t>{0, 1, 2, 3, 2, 1};

class Client : net::Client {
public:
  struct Create_info {
    net::Client::Create_info client_info;
    glfw::Window glfw_window;
    vk::SurfaceKHR vk_surface;
  };

  explicit Client(const Create_info &create_info)
      : net::Client{create_info.client_info},
        _glfw_window{create_info.glfw_window}, _vk_surface{
                                                   create_info.vk_surface} {
    _vma_allocator = make_vma_allocator();
    _vk_command_pool =
        Global_vulkan_state::get().device().createCommandPoolUnique({
            .flags = vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = Global_vulkan_state::get().queue_family_index(),
        });
    _floor_vertex_buffer =
        upload_vertices(std::as_bytes(std::span{floor_mesh_vertices}));
    std::cout << "Uploaded floor vertex buffer.\n";
    _floor_index_buffer =
        upload_indices(std::as_bytes(std::span{floor_mesh_indices}));
    std::cout << "Uploaded floor index buffer.\n";
    _vk_image_acquire_semaphore = make_vk_semaphore("image_acquire_semaphore");
    _vk_image_release_semaphore = make_vk_semaphore("image_release_semaphore");
    _vk_work_done_fence =
        Global_vulkan_state::get().device().createFenceUnique({
            .flags = vk::FenceCreateFlagBits::eSignaled,
        });
    std::cout << "Created per-frame Vulkan synchronization primitives.\n";
    _vk_command_buffer = std::move(
        Global_vulkan_state::get().device().allocateCommandBuffersUnique({
            .commandPool = *_vk_command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        })[0]);
    std::cout << "Allocated per-frame VkCommandBuffer.\n";
    std::tie(_vk_swapchain, _vk_swapchain_image_format,
             _vk_swapchain_image_extent) = make_vk_swapchain();
    std::cout << "Created VkSwapchainKHR.\n";
    _vk_swapchain_images =
        Global_vulkan_state::get().device().getSwapchainImagesKHR(
            *_vk_swapchain);
    std::cout << "Got Vulkan swapchain images.\n";
    _vk_swapchain_image_views = make_vk_swapchain_image_views();
    std::cout << "Created Vulkan swapchain image views.\n";
    _vk_pipeline_layout = make_vk_pipeline_layout();
    std::cout << "Created VkPipelineLayout.\n";
    _vk_pipeline = make_vk_pipeline();
    std::cout << "Created VkPipeline.\n";
    _game = game::create_replicated_game_unique({});
  }

  void poll_network_events() { net::Client::poll_events(); }

  void wait_network_events(std::uint32_t timeout) {
    net::Client::wait_events(timeout);
  }

  void connect(const enet::Address &address) { net::Client::connect(address); }

  bool is_connecting() const noexcept { return net::Client::is_connecting(); }

  bool is_connected() const noexcept { return net::Client::is_connected(); }

  void send_player_input_state(const game::Player::Input_state &input_state) {
    net::Client::send_player_input_state(input_state);
  }

  void render() {
    try {
      const auto vk_swapchain_image_index =
          Global_vulkan_state::get()
              .device()
              .acquireNextImageKHR(*_vk_swapchain,
                                   std::numeric_limits<std::uint64_t>::max(),
                                   *_vk_image_acquire_semaphore, {})
              .value;
      std::ignore = Global_vulkan_state::get().device().waitForFences(
          1, &*_vk_work_done_fence, true,
          std::numeric_limits<std::uint64_t>::max());
      std::ignore = Global_vulkan_state::get().device().resetFences(
          1, &*_vk_work_done_fence);
      Global_vulkan_state::get().device().resetCommandPool(*_vk_command_pool);
      record_vk_command_buffer(
          _vk_swapchain_images[vk_swapchain_image_index],
          *_vk_swapchain_image_views[vk_swapchain_image_index]);
      const auto vk_image_acquire_wait_stage =
          vk::PipelineStageFlags{vk::PipelineStageFlagBits::eTopOfPipe};
      Global_vulkan_state::get().queue().submit(
          {{
              .waitSemaphoreCount = 1,
              .pWaitSemaphores = &*_vk_image_acquire_semaphore,
              .pWaitDstStageMask = &vk_image_acquire_wait_stage,
              .commandBufferCount = 1,
              .pCommandBuffers = &*_vk_command_buffer,
              .signalSemaphoreCount = 1,
              .pSignalSemaphores = &*_vk_image_release_semaphore,
          }},
          *_vk_work_done_fence);
      const auto vk_queue_present_result =
          Global_vulkan_state::get().queue().presentKHR({
              .waitSemaphoreCount = 1,
              .pWaitSemaphores = &*_vk_image_release_semaphore,
              .swapchainCount = 1,
              .pSwapchains = &*_vk_swapchain,
              .pImageIndices = &vk_swapchain_image_index,
          });
      if (vk_queue_present_result == vk::Result::eSuboptimalKHR) {
        throw vk::OutOfDateKHRError{"Subobtimal queue present result"};
      }
    } catch (const vk::OutOfDateKHRError &e) {
      Global_vulkan_state::get().device().waitIdle();
      _vk_swapchain_image_views.clear();
      _vk_swapchain_images.clear();
      _vk_swapchain.reset();
      const auto previous_vk_swapchain_image_format =
          _vk_swapchain_image_format;
      std::tie(_vk_swapchain, _vk_swapchain_image_format,
               _vk_swapchain_image_extent) = make_vk_swapchain();
      _vk_swapchain_images =
          Global_vulkan_state::get().device().getSwapchainImagesKHR(
              *_vk_swapchain);
      _vk_swapchain_image_views = make_vk_swapchain_image_views();
      if (_vk_swapchain_image_format != previous_vk_swapchain_image_format) {
        std::cout << "Swapchain changed image format.\n";
        _vk_pipeline = make_vk_pipeline();
      }
    }
  }

  constexpr glfw::Window get_window() const noexcept { return _glfw_window; }

protected:
  void on_disconnect() override { _player_id = std::nullopt; }

  void on_player_id(std::uint32_t player_id) override {
    _player_id = player_id;
    std::cout << "Got player id: " << player_id << ".\n";
  }

  void on_game_state(serial::Reader &reader) override {
    _game->apply_snapshot(reader);
    const auto player =
        _player_id ? _game->get_player(*_player_id) : game::Replicated_player{};
    if (player) {
      const auto &player_position = player.get_position();
      std::cout << "Client player position: (" << player_position.x() << ", "
                << player_position.y() << ", " << player_position.z() << ").\n";
    }
  }

private:
  vma::Unique_allocator make_vma_allocator() {
    auto create_info = vma::Allocator::Create_info{
        .flags = {},
        .physicalDevice = Global_vulkan_state::get().physical_device(),
        .device = Global_vulkan_state::get().device(),
        .preferredLargeHeapBlockSize = 0,
        .pAllocationCallbacks = nullptr,
        .pDeviceMemoryCallbacks = nullptr,
        .pHeapSizeLimit = nullptr,
        .pVulkanFunctions = nullptr,
        .instance = Global_vulkan_state::get().instance(),
        .vulkanApiVersion = vk::ApiVersion13,
        .pTypeExternalMemoryHandleTypes = nullptr,
    };
    const auto vulkan_functions = vma::import_functions_from_volk(create_info);
    create_info.pVulkanFunctions = &vulkan_functions;
    auto retval = vma::create_allocator_unique(create_info);
    std::cout << "Created VmaAllocator.\n";
    return retval;
  }

  Vertex_buffer upload_vertices(std::span<const std::byte> data) {
    return Vertex_buffer{*_vma_allocator, *_vk_command_pool, data};
  }

  Index_buffer upload_indices(std::span<const std::byte> data) {
    return Index_buffer{*_vma_allocator, *_vk_command_pool, data};
  }

  vk::UniqueSemaphore make_vk_semaphore(const char *debug_name) {
    auto retval = Global_vulkan_state::get().device().createSemaphoreUnique({});
    Global_vulkan_state::get().device().setDebugUtilsObjectNameEXT(
        {.objectType = vk::ObjectType::eSemaphore,
         .objectHandle = std::bit_cast<std::uint64_t>(*retval),
         .pObjectName = debug_name});
    return retval;
  }

  std::tuple<vk::UniqueSwapchainKHR, vk::Format, vk::Extent2D>
  make_vk_swapchain() {
    const auto capabilities =
        Global_vulkan_state::get().physical_device().getSurfaceCapabilitiesKHR(
            _vk_surface);
    const auto image_count = capabilities.maxImageCount > 0
                                 ? std::min(capabilities.maxImageCount,
                                            capabilities.minImageCount + 1)
                                 : (capabilities.minImageCount + 1);
    const auto surface_format = [&]() {
      const auto surface_formats =
          Global_vulkan_state::get().physical_device().getSurfaceFormatsKHR(
              _vk_surface);
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
        const auto framebuffer_size = _glfw_window.get_framebuffer_size();
        return vk::Extent2D{
            .width = std::clamp(static_cast<std::uint32_t>(framebuffer_size[0]),
                                capabilities.minImageExtent.width,
                                capabilities.maxImageExtent.width),
            .height =
                std::clamp(static_cast<std::uint32_t>(framebuffer_size[1]),
                           capabilities.minImageExtent.height,
                           capabilities.maxImageExtent.height)};
      }
    }();
    auto const present_mode = [&]() {
      const auto present_modes = Global_vulkan_state::get()
                                     .physical_device()
                                     .getSurfacePresentModesKHR(_vk_surface);
      for (const auto present_mode : present_modes) {
        if (present_mode == vk::PresentModeKHR::eMailbox) {
          return present_mode;
        }
      }
      return vk::PresentModeKHR::eFifo;
    }();
    return std::tuple{
        Global_vulkan_state::get().device().createSwapchainKHRUnique({
            .surface = _vk_surface,
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

  std::vector<vk::UniqueImageView> make_vk_swapchain_image_views() {
    auto retval = std::vector<vk::UniqueImageView>{};
    retval.reserve(_vk_swapchain_images.size());
    for (const auto image : _vk_swapchain_images) {
      retval.emplace_back(
          Global_vulkan_state::get().device().createImageViewUnique(
              {.image = image,
               .viewType = vk::ImageViewType::e2D,
               .format = _vk_swapchain_image_format,
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

  vk::UniquePipelineLayout make_vk_pipeline_layout() {
    auto push_constant_range = vk::PushConstantRange{};
    push_constant_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_constant_range.offset = 0;
    push_constant_range.size = 64;
    return Global_vulkan_state::get().device().createPipelineLayoutUnique({
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    });
  }

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

  vk::UniquePipeline make_vk_pipeline() {
    const auto vert_shader_module =
        load_vk_shader_module("./assets/shaders/shader.vert.spv");
    const auto frag_shader_module =
        load_vk_shader_module("./assets/shaders/shader.frag.spv");
    const auto shader_stages = std::vector<vk::PipelineShaderStageCreateInfo>{
        {.stage = vk::ShaderStageFlagBits::eVertex,
         .module = *vert_shader_module,
         .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eFragment,
         .module = *frag_shader_module,
         .pName = "main"}};
    const auto vertex_binding = vk::VertexInputBindingDescription{
        .binding = 0,
        .stride = 6 * sizeof(float),
        .inputRate = vk::VertexInputRate::eVertex};
    const auto vertex_attributes =
        std::vector<vk::VertexInputAttributeDescription>{
            {.location = 0,
             .binding = 0,
             .format = vk::Format::eR32G32B32Sfloat,
             .offset = 0},
            {.location = 1,
             .binding = 0,
             .format = vk::Format::eR32G32B32Sfloat,
             .offset = 3 * sizeof(float)}};
    const auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding,
        .vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(vertex_attributes.size()),
        .pVertexAttributeDescriptions = vertex_attributes.data()};
    const auto input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = vk::False};
    const auto rasterization_state = vk::PipelineRasterizationStateCreateInfo{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f};
    const auto blend_attachment = vk::PipelineColorBlendAttachmentState{
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
    const auto color_blend_state = vk::PipelineColorBlendStateCreateInfo{
        .attachmentCount = 1, .pAttachments = &blend_attachment};
    const auto viewport_state = vk::PipelineViewportStateCreateInfo{
        .viewportCount = 1, .scissorCount = 1};
    const auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        .depthCompareOp = vk::CompareOp::eAlways};
    const auto multisample_state = vk::PipelineMultisampleStateCreateInfo{
        .rasterizationSamples = vk::SampleCountFlagBits::e1};
    const auto dynamic_states = std::vector<vk::DynamicState>{
        vk::DynamicState::eViewport, vk::DynamicState::eScissor,
        vk::DynamicState::eCullMode, vk::DynamicState::eFrontFace,
        vk::DynamicState::ePrimitiveTopology};
    const auto dynamic_state = vk::PipelineDynamicStateCreateInfo{
        .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data()};
    const auto rendering_info = vk::PipelineRenderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &_vk_swapchain_image_format};
    return std::move(
        Global_vulkan_state::get()
            .device()
            .createGraphicsPipelinesUnique(
                {}, {vk::GraphicsPipelineCreateInfo{
                        .pNext = &rendering_info,
                        .stageCount =
                            static_cast<std::uint32_t>(shader_stages.size()),
                        .pStages = shader_stages.data(),
                        .pVertexInputState = &vertex_input_state,
                        .pInputAssemblyState = &input_assembly_state,
                        .pViewportState = &viewport_state,
                        .pRasterizationState = &rasterization_state,
                        .pMultisampleState = &multisample_state,
                        .pDepthStencilState = &depth_stencil_state,
                        .pColorBlendState = &color_blend_state,
                        .pDynamicState = &dynamic_state,
                        .layout = *_vk_pipeline_layout,
                    }})
            .value[0]);
  }

  void record_vk_command_buffer(vk::Image swapchain_image,
                                vk::ImageView swapchain_image_view) {
    _vk_command_buffer->begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
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
    _vk_command_buffer->pipelineBarrier2({
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
    _vk_command_buffer->beginRendering({
        .renderArea = {.offset = {0, 0}, .extent = _vk_swapchain_image_extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
    });
    const auto player =
        _player_id ? _game->get_player(*_player_id) : game::Replicated_player{};
    if (player) {
      _vk_command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                       *_vk_pipeline);
      _vk_command_buffer->setViewport(
          0,
          {{
              .width = static_cast<float>(_vk_swapchain_image_extent.width),
              .height = static_cast<float>(_vk_swapchain_image_extent.height),
              .minDepth = 0.0f,
              .maxDepth = 1.0f,
          }});
      _vk_command_buffer->setScissor(0,
                                     {{.extent = _vk_swapchain_image_extent}});
      _vk_command_buffer->setCullMode(vk::CullModeFlagBits::eNone);
      _vk_command_buffer->setFrontFace(vk::FrontFace::eClockwise);
      _vk_command_buffer->setPrimitiveTopology(
          vk::PrimitiveTopology::eTriangleList);
      _vk_command_buffer->bindVertexBuffers(
          0, {_floor_vertex_buffer.get_buffer()}, {0});
      _vk_command_buffer->bindIndexBuffer(_floor_index_buffer.get_buffer(), 0,
                                          vk::IndexType::eUint16);
      const auto view_matrix = translation_matrix(-player.get_position());
      const auto projection_matrix =
          perspective_projection_matrix(1.0f, 1.0f, 0.01f);
      const auto view_projection_matrix =
          Eigen::Matrix4f{projection_matrix * view_matrix};
      _vk_command_buffer->pushConstants(*_vk_pipeline_layout,
                                        vk::ShaderStageFlagBits::eVertex, 0, 64,
                                        view_projection_matrix.data());
      _vk_command_buffer->drawIndexed(floor_mesh_indices.size(), 1, 0, 0, 0);
    }
    _vk_command_buffer->endRendering();
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
    _vk_command_buffer->pipelineBarrier2({
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &swapchain_image_barrier_2,
    });
    _vk_command_buffer->end();
  }

  glfw::Window _glfw_window{};
  vk::SurfaceKHR _vk_surface{};
  vma::Unique_allocator _vma_allocator{};
  vk::UniqueCommandPool _vk_command_pool{};
  Vertex_buffer _floor_vertex_buffer{};
  Index_buffer _floor_index_buffer{};
  vk::UniqueSemaphore _vk_image_acquire_semaphore{};
  vk::UniqueSemaphore _vk_image_release_semaphore{};
  vk::UniqueFence _vk_work_done_fence{};
  vk::UniqueCommandBuffer _vk_command_buffer{};
  vk::UniqueSwapchainKHR _vk_swapchain{};
  vk::Format _vk_swapchain_image_format{};
  vk::Extent2D _vk_swapchain_image_extent{};
  std::vector<vk::Image> _vk_swapchain_images{};
  std::vector<vk::UniqueImageView> _vk_swapchain_image_views{};
  vk::UniquePipelineLayout _vk_pipeline_layout{};
  vk::UniquePipeline _vk_pipeline{};
  game::Unique_replicated_game _game{};
  std::optional<std::uint32_t> _player_id{};
};

} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto enet_guard = enet::Initialization_guard{{}};
  const auto glfw_guard = glfw::Initialization_guard{{}};
  const auto glfw_window = glfw::create_window_unique({
      .width = 1024,
      .height = 768,
      .title = "FPS Party",
      .resizable = false,
      .client_api = glfw::Client_api::no_api,
  });
  std::cout << "Opened window.\n";
  const auto vulkan_guard = Global_vulkan_state_guard{{}};
  const auto vk_surface = make_vk_surface(*glfw_window);
  auto client = Client{{
      .client_info = {},
      .glfw_window = *glfw_window,
      .vk_surface = *vk_surface,
  }};
  client.connect({
      .host = *enet::parse_ip(server_ip),
      .port = constants::port,
  });
  std::cout << "Connecting to " << server_ip << " on port " << constants::port
            << ".\n";
  while (client.is_connecting()) {
    client.wait_network_events(100);
  }
  if (!client.is_connected()) {
    std::cout << "Connection failed.\n";
    return 0;
  }
  std::cout << "Connection successful.\n";
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;
  auto input_duration = Duration{};
  auto input_time = Clock::now();
  while (!signal_status && !client.get_window().should_close() &&
         client.is_connected()) {
    glfw::poll_events();
    const auto now = Clock::now();
    input_duration += now - input_time;
    input_time = now;
    if (input_duration >=
        std::chrono::duration<float>(constants::input_duration)) {
      input_duration -= std::chrono::duration_cast<Duration>(
          std::chrono::duration<float>(constants::input_duration));
      client.send_player_input_state(game::Player::Input_state{
          .move_left = client.get_window().get_key(glfw::Key::k_a) ==
                       glfw::Key_state::press,
          .move_right = client.get_window().get_key(glfw::Key::k_d) ==
                        glfw::Key_state::press,
          .move_forward = client.get_window().get_key(glfw::Key::k_w) ==
                          glfw::Key_state::press,
          .move_backward = client.get_window().get_key(glfw::Key::k_s) ==
                           glfw::Key_state::press,
      });
    }
    client.poll_network_events();
    client.render();
  }
  Global_vulkan_state::get().device().waitIdle();
  std::cout << "Exiting.\n";
  return 0;
}
