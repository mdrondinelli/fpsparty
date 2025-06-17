#include "client/index_buffer.hpp"
#include "client/perspective_projection.hpp"
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
const auto vk_device_extensions = std::array{vk::KHRSwapchainExtensionName};
constexpr auto server_ip = "127.0.0.1";

volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }

class Bad_vk_shader_module_size_error : std::exception {};

struct Vertex {
  float x;
  float y;
  float z;
  float r;
  float g;
  float b;
};

class Client : net::Client {
public:
  struct Create_info {
    net::Client::Create_info client;
    glfw::Window::Create_info window;
  };

  explicit Client(const Create_info &create_info)
      : net::Client{create_info.client} {
    _window = make_window(create_info.window);
    _vk_instance = make_vk_instance();
    _vk_surface = make_vk_surface();
    std::tie(_vk_physical_device, _vk_queue_family_index) =
        find_vk_physical_device();
    std::tie(_vk_device, _vk_queue) = make_vk_device();
    _vma_allocator = make_vma_allocator();
    _vk_command_pool = _vk_device->createCommandPoolUnique(
        {.flags = vk::CommandPoolCreateFlagBits::eTransient,
         .queueFamilyIndex = _vk_queue_family_index});
    const auto floor_vertices = std::vector<Vertex>{
        {.x = 10.0f, .y = -0.5f, .z = 10.0f, .r = 1.0f, .g = 0.0f, .b = 0.0f},
        {.x = -10.0f, .y = -0.5f, .z = 10.0f, .r = 0.0f, .g = 1.0f, .b = 0.0f},
        {.x = 10.0f, .y = -0.5f, .z = -10.0f, .r = 0.0f, .g = 0.0f, .b = 1.0f},
        {.x = -10.0f, .y = -0.5f, .z = -10.0f, .r = 1.0f, .g = 1.0f, .b = 0.0f},
    };
    const auto floor_indices = std::vector<std::uint16_t>{0, 1, 2, 2, 3, 1};
    _floor_vertex_buffer =
        upload_vertices(std::as_bytes(std::span{floor_vertices}));
    std::cout << "Uploaded vertex buffer.\n";
    _floor_index_buffer =
        upload_indices(std::as_bytes(std::span{floor_indices}));
    std::cout << "Uploaded index buffer.\n";
    _vk_image_acquire_semaphore = make_vk_semaphore("image_acquire_semaphore");
    _vk_image_release_semaphore = make_vk_semaphore("image_release_semaphore");
    _vk_work_done_fence = _vk_device->createFenceUnique(
        {.flags = vk::FenceCreateFlagBits::eSignaled});
    std::cout << "Created per-frame Vulkan synchronization primitives.\n";
    _vk_command_buffer = std::move(_vk_device->allocateCommandBuffersUnique(
        {.commandPool = *_vk_command_pool,
         .level = vk::CommandBufferLevel::ePrimary,
         .commandBufferCount = 1})[0]);
    std::cout << "Allocated per-frame VkCommandBuffer.\n";
    std::tie(_vk_swapchain, _vk_swapchain_image_format,
             _vk_swapchain_image_extent) = make_vk_swapchain();
    std::cout << "Created VkSwapchainKHR.\n";
    _vk_swapchain_images = _vk_device->getSwapchainImagesKHR(*_vk_swapchain);
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
          _vk_device
              ->acquireNextImageKHR(*_vk_swapchain,
                                    std::numeric_limits<std::uint64_t>::max(),
                                    *_vk_image_acquire_semaphore, {})
              .value;
      std::ignore =
          _vk_device->waitForFences(1, &*_vk_work_done_fence, true,
                                    std::numeric_limits<std::uint64_t>::max());
      std::ignore = _vk_device->resetFences(1, &*_vk_work_done_fence);
      _vk_device->resetCommandPool(*_vk_command_pool);
      record_vk_command_buffer(
          _vk_swapchain_images[vk_swapchain_image_index],
          *_vk_swapchain_image_views[vk_swapchain_image_index]);
      const auto vk_image_acquire_wait_stage =
          vk::PipelineStageFlags{vk::PipelineStageFlagBits::eTopOfPipe};
      _vk_queue.submit({{
                           .waitSemaphoreCount = 1,
                           .pWaitSemaphores = &*_vk_image_acquire_semaphore,
                           .pWaitDstStageMask = &vk_image_acquire_wait_stage,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &*_vk_command_buffer,
                           .signalSemaphoreCount = 1,
                           .pSignalSemaphores = &*_vk_image_release_semaphore,
                       }},
                       *_vk_work_done_fence);
      const auto vk_queue_present_result = _vk_queue.presentKHR({
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
      _vk_device->waitIdle();
      _vk_swapchain_image_views.clear();
      _vk_swapchain_images.clear();
      _vk_swapchain.reset();
      const auto previous_vk_swapchain_image_format =
          _vk_swapchain_image_format;
      std::tie(_vk_swapchain, _vk_swapchain_image_format,
               _vk_swapchain_image_extent) = make_vk_swapchain();
      _vk_swapchain_images = _vk_device->getSwapchainImagesKHR(*_vk_swapchain);
      _vk_swapchain_image_views = make_vk_swapchain_image_views();
      if (_vk_swapchain_image_format != previous_vk_swapchain_image_format) {
        std::cout << "Swapchain changed image format.\n";
        _vk_pipeline = make_vk_pipeline();
      }
    }
  }

  void wait_until_gpu_idle() { _vk_device->waitIdle(); }

  constexpr glfw::Window get_window() const noexcept { return *_window; }

protected:
  void on_disconnect() override { _player_id = std::nullopt; }

  void on_player_id(std::uint32_t player_id) override {
    _player_id = player_id;
    std::cout << "Got player id: " << player_id << ".\n";
  }

  void on_game_state(serial::Reader &reader) override {
    _game->apply_snapshot(reader);
    if (_player_id) {
      const auto player = _game->get_player(*_player_id);
      const auto &player_position = player.get_position();
      std::cout << "Client player position: (" << player_position.x() << ", "
                << player_position.y() << ", " << player_position.z() << ").\n";
    }
  }

private:
  glfw::Unique_window
  make_window(const glfw::Window::Create_info &create_info) {
    auto retval = glfw::create_window_unique(create_info);
    std::cout << "Opened window.\n";
    return retval;
  }

  vk::UniqueInstance make_vk_instance() {
    volkInitialize();
    const auto app_info = vk::ApplicationInfo{
        .pApplicationName = "FPS Party",
        .pEngineName = "FPS Party",
        .apiVersion = vk::ApiVersion13,
    };
#ifndef NDEBUG
    std::cout << "Enabling Vulkan validation layers.\n";
    const auto layers = std::array{"VK_LAYER_KHRONOS_validation"};
#endif
#ifndef NDEBUG
    const auto glfw_extensions = glfw::get_required_instance_extensions();
    std::cout << "Enabling Vulkan debug extension.\n";
    auto extensions =
        std::vector<const char *>(std::from_range, glfw_extensions);
    extensions.push_back(vk::EXTDebugUtilsExtensionName);
#else
    const auto extensions = glfw::get_required_instance_extensions();
#endif
    const auto create_info = vk::InstanceCreateInfo{
        .pApplicationInfo = &app_info,
#ifndef NDEBUG
        .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
#endif
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
    const auto vkGetInstanceProcAddr =
        vk::DynamicLoader{}.getProcAddress<PFN_vkGetInstanceProcAddr>(
            "vkGetInstanceProcAddr");
    vk::defaultDispatchLoaderDynamic.init(vkGetInstanceProcAddr);
    auto instance = vk::createInstanceUnique(create_info);
    volkLoadInstance(*instance);
    vk::defaultDispatchLoaderDynamic.init(*instance, vkGetInstanceProcAddr);
    std::cout << "Created VkInstance.\n";
    return instance;
  }

  vk::UniqueSurfaceKHR make_vk_surface() {
    auto retval = glfw::create_window_surface_unique(*_vk_instance, *_window);
    std::cout << "Created VkSurfaceKHR.\n";
    return retval;
  }

  /**
   * @return a tuple with a physical device and a queue family index
   */
  std::tuple<vk::PhysicalDevice, std::uint32_t> find_vk_physical_device() {
    const auto physical_devices = _vk_instance->enumeratePhysicalDevices();
    for (const auto physical_device : physical_devices) {
      const auto properties = physical_device.getProperties();
      if (properties.apiVersion < vk::ApiVersion13) {
        std::cout << "Skipping VkPhysicalDevice '" << properties.deviceName
                  << "' because it does not support Vulkan 1.3.\n";
        continue;
      }
      const auto queue_family_index = [&]() -> std::optional<std::uint32_t> {
        const auto queue_families = physical_device.getQueueFamilyProperties();
        for (auto i = std::uint32_t{}; i != queue_families.size(); ++i) {
          if ((queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
              physical_device.getSurfaceSupportKHR(i, *_vk_surface)) {
            return i;
          }
        }
        return std::nullopt;
      }();
      if (!queue_family_index) {
        std::cout << "Skipping VkPhysicalDevice '" << properties.deviceName
                  << "' because it does not have a queue family that supports "
                     "graphics and presentation.\n";
        continue;
      }
      const auto has_extensions = [&]() {
        const auto available_extensions =
            physical_device.enumerateDeviceExtensionProperties();
        for (const auto extension : vk_device_extensions) {
          auto extension_found = false;
          for (const auto &available_extension : available_extensions) {
            if (std::strcmp(available_extension.extensionName, extension) ==
                0) {
              extension_found = true;
              break;
            }
          }
          if (!extension_found) {
            return false;
          }
        }
        return true;
      }();
      if (!has_extensions) {
        std::cout << "Skipping VkPhysicalDevice '" << properties.deviceName
                  << "' because it does not have a queue family that supports "
                     "graphics and presentation.\n";
        continue;
      }
      if (physical_device.getSurfaceFormatsKHR(*_vk_surface).empty()) {
        std::cout
            << "Skipping VkPhysicalDevice '" << properties.deviceName
            << "' because it does not support any surface formats for the "
               "window surface.\n";
        continue;
      }
      if (physical_device.getSurfacePresentModesKHR(*_vk_surface).empty()) {
        std::cout << "Skipping VkPhysicalDevice '" << properties.deviceName
                  << "' because it does not support any present modes for the "
                     "window surface.\n";
        continue;
      }
      std::cout << "Found viable VkPhysicalDevice '" << properties.deviceName
                << "'.\n";
      return std::tuple{physical_device, *queue_family_index};
    }
    std::cout
        << "Throwing an error because no viable VkPhysicalDevice was found.\n";
    throw std::runtime_error{"No viable VkPhysicalDevice was found."};
  }

  std::tuple<vk::UniqueDevice, vk::Queue> make_vk_device() {
    const auto queue_priority = 1.0f;
    const auto queue_create_info = vk::DeviceQueueCreateInfo{
        .queueFamilyIndex = _vk_queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    auto extended_dynamic_state_features =
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{
            .extendedDynamicState = true,
        };
    auto vulkan_1_3_features = vk::PhysicalDeviceVulkan13Features{
        .pNext = &extended_dynamic_state_features,
        .synchronization2 = true,
        .dynamicRendering = true,
    };
    const auto features = vk::PhysicalDeviceFeatures2{
        .pNext = &vulkan_1_3_features,
    };
    auto device = _vk_physical_device.createDeviceUnique({
        .pNext = &features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount =
            static_cast<std::uint32_t>(vk_device_extensions.size()),
        .ppEnabledExtensionNames = vk_device_extensions.data(),
    });
    volkLoadDevice(*device);
    const auto queue = device->getQueue(_vk_queue_family_index, 0);
    std::cout << "Created VkDevice.\n";
    return std::tuple{std::move(device), queue};
  }

  vma::Unique_allocator make_vma_allocator() {
    auto create_info =
        vma::Allocator::Create_info{.flags = {},
                                    .physicalDevice = _vk_physical_device,
                                    .device = *_vk_device,
                                    .preferredLargeHeapBlockSize = 0,
                                    .pAllocationCallbacks = nullptr,
                                    .pDeviceMemoryCallbacks = nullptr,
                                    .pHeapSizeLimit = nullptr,
                                    .pVulkanFunctions = nullptr,
                                    .instance = *_vk_instance,
                                    .vulkanApiVersion = vk::ApiVersion13,
                                    .pTypeExternalMemoryHandleTypes = nullptr};
    const auto vulkan_functions = vma::import_functions_from_volk(create_info);
    create_info.pVulkanFunctions = &vulkan_functions;
    auto retval = vma::create_allocator_unique(create_info);
    std::cout << "Created VmaAllocator.\n";
    return retval;
  }

  Vertex_buffer upload_vertices(std::span<const std::byte> data) {
    return Vertex_buffer{*_vma_allocator, *_vk_device, _vk_queue,
                         *_vk_command_pool, data};
  }

  Index_buffer upload_indices(std::span<const std::byte> data) {
    return Index_buffer{*_vma_allocator, *_vk_device, _vk_queue,
                        *_vk_command_pool, data};
  }

  vk::UniqueSemaphore make_vk_semaphore(const char *debug_name) {
    auto retval = _vk_device->createSemaphoreUnique({});
    _vk_device->setDebugUtilsObjectNameEXT(
        {.objectType = vk::ObjectType::eSemaphore,
         .objectHandle = std::bit_cast<std::uint64_t>(*retval),
         .pObjectName = debug_name});
    return retval;
  }

  std::tuple<vk::UniqueSwapchainKHR, vk::Format, vk::Extent2D>
  make_vk_swapchain() {
    const auto capabilities =
        _vk_physical_device.getSurfaceCapabilitiesKHR(*_vk_surface);
    const auto image_count = capabilities.maxImageCount > 0
                                 ? std::min(capabilities.maxImageCount,
                                            capabilities.minImageCount + 1)
                                 : (capabilities.minImageCount + 1);
    const auto surface_format = [&]() {
      const auto surface_formats =
          _vk_physical_device.getSurfaceFormatsKHR(*_vk_surface);
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
        const auto framebuffer_size = _window->get_framebuffer_size();
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
      const auto present_modes =
          _vk_physical_device.getSurfacePresentModesKHR(*_vk_surface);
      for (const auto present_mode : present_modes) {
        if (present_mode == vk::PresentModeKHR::eMailbox) {
          return present_mode;
        }
      }
      return vk::PresentModeKHR::eFifo;
    }();
    return std::tuple{
        _vk_device->createSwapchainKHRUnique({
            .surface = *_vk_surface,
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
      retval.emplace_back(_vk_device->createImageViewUnique(
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
    return _vk_device->createPipelineLayoutUnique(
        {.pushConstantRangeCount = 1,
         .pPushConstantRanges = &push_constant_range});
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
    return _vk_device->createShaderModuleUnique(
        {.codeSize = static_cast<std::uint32_t>(size),
         .pCode = bytecode.data()});
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
        _vk_device
            ->createGraphicsPipelinesUnique(
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
                        .layout = *_vk_pipeline_layout}})
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
        .clearValue = {{1.0f, 0.0f, 1.0f, 1.0f}},
    };
    _vk_command_buffer->beginRendering({
        .renderArea = {.offset = {0, 0}, .extent = _vk_swapchain_image_extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
    });
    _vk_command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                     *_vk_pipeline);
    _vk_command_buffer->setViewport(
        0, {{
               .width = static_cast<float>(_vk_swapchain_image_extent.width),
               .height = static_cast<float>(_vk_swapchain_image_extent.height),
               .minDepth = 0.0f,
               .maxDepth = 1.0f,
           }});
    _vk_command_buffer->setScissor(0, {{.extent = _vk_swapchain_image_extent}});
    _vk_command_buffer->setCullMode(vk::CullModeFlagBits::eNone);
    _vk_command_buffer->setFrontFace(vk::FrontFace::eClockwise);
    _vk_command_buffer->setPrimitiveTopology(
        vk::PrimitiveTopology::eTriangleList);
    _vk_command_buffer->bindVertexBuffers(
        0, {_floor_vertex_buffer.get_buffer()}, {0});
    _vk_command_buffer->bindIndexBuffer(_floor_index_buffer.get_buffer(), 0,
                                        vk::IndexType::eUint16);
    const auto projection_matrix =
        make_perspective_projection_matrix(1.0f, 1.0f, 0.01f);
    _vk_command_buffer->pushConstants(*_vk_pipeline_layout,
                                      vk::ShaderStageFlagBits::eVertex, 0, 64,
                                      projection_matrix.data());
    const auto floor_index_count = 6;
    _vk_command_buffer->drawIndexed(floor_index_count, 1, 0, 0, 0);
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

  glfw::Unique_window _window{};
  vk::UniqueInstance _vk_instance{};
  vk::UniqueSurfaceKHR _vk_surface{};
  vk::PhysicalDevice _vk_physical_device{};
  std::uint32_t _vk_queue_family_index{};
  vk::UniqueDevice _vk_device{};
  vk::Queue _vk_queue{};
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
  auto client = Client{{
      .client = {},
      .window =
          {
              .width = 1024,
              .height = 768,
              .title = "FPS Party",
              .resizable = false,
              .client_api = glfw::Client_api::no_api,
          },
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
  client.wait_until_gpu_idle();
  std::cout << "Exiting.\n";
  return 0;
}
