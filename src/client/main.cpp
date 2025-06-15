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
  auto extensions = std::vector<const char *>(std::from_range, glfw_extensions);
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
  return instance;
}

/**
 * @return a tuple with a physical device and a queue family index
 */
std::tuple<vk::PhysicalDevice, std::uint32_t>
find_vk_physical_device(vk::Instance instance, vk::SurfaceKHR surface) {
  const auto physical_devices = instance.enumeratePhysicalDevices();
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
            physical_device.getSurfaceSupportKHR(i, surface)) {
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
          if (std::strcmp(available_extension.extensionName, extension) == 0) {
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
    if (physical_device.getSurfaceFormatsKHR(surface).empty()) {
      std::cout << "Skipping VkPhysicalDevice '" << properties.deviceName
                << "' because it does not support any surface formats for the "
                   "window surface.\n";
      continue;
    }
    if (physical_device.getSurfacePresentModesKHR(surface).empty()) {
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

std::tuple<vk::UniqueDevice, vk::Queue>
make_vk_device(vk::PhysicalDevice physical_device,
               std::uint32_t queue_family_index) {
  const auto queue_priority = 1.0f;
  const auto queue_create_info = vk::DeviceQueueCreateInfo{
      .queueFamilyIndex = queue_family_index,
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
  auto device = physical_device.createDeviceUnique({
      .pNext = &features,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queue_create_info,
      .enabledExtensionCount =
          static_cast<std::uint32_t>(vk_device_extensions.size()),
      .ppEnabledExtensionNames = vk_device_extensions.data(),
  });
  volkLoadDevice(*device);
  const auto queue = device->getQueue(queue_family_index, 0);
  return std::tuple{std::move(device), queue};
}

vma::Unique_allocator make_vma_allocator(vk::Instance instance,
                                         vk::PhysicalDevice physical_device,
                                         vk::Device device) {
  auto create_info =
      vma::Allocator::Create_info{.flags = {},
                                  .physicalDevice = physical_device,
                                  .device = device,
                                  .preferredLargeHeapBlockSize = 0,
                                  .pAllocationCallbacks = nullptr,
                                  .pDeviceMemoryCallbacks = nullptr,
                                  .pHeapSizeLimit = nullptr,
                                  .pVulkanFunctions = nullptr,
                                  .instance = instance,
                                  .vulkanApiVersion = vk::ApiVersion13,
                                  .pTypeExternalMemoryHandleTypes = nullptr};
  const auto vulkan_functions = vma::import_functions_from_volk(create_info);
  create_info.pVulkanFunctions = &vulkan_functions;
  return vma::create_allocator_unique(create_info);
}

struct Vertex {
  float x;
  float y;
  float r;
  float g;
  float b;
};

Vertex_buffer upload_vertex_buffer(vma::Allocator allocator, vk::Device device,
                                   vk::Queue queue,
                                   vk::CommandPool command_pool,
                                   std::span<const Vertex> vertices) {
  return Vertex_buffer{allocator,    device,          queue,
                       command_pool, vertices.data(), vertices.size_bytes()};
}

std::tuple<vk::UniqueSwapchainKHR, vk::Format, vk::Extent2D>
make_vk_swapchain(vk::PhysicalDevice physical_device, vk::Device device,
                  glfw::Window window, vk::SurfaceKHR surface) {
  const auto capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
  const auto image_count =
      capabilities.maxImageCount > 0
          ? std::min(capabilities.maxImageCount, capabilities.minImageCount + 1)
          : (capabilities.minImageCount + 1);
  const auto surface_format = [&]() {
    const auto surface_formats = physical_device.getSurfaceFormatsKHR(surface);
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
        physical_device.getSurfacePresentModesKHR(surface);
    for (const auto present_mode : present_modes) {
      if (present_mode == vk::PresentModeKHR::eMailbox) {
        return present_mode;
      }
    }
    return vk::PresentModeKHR::eFifo;
  }();
  return std::tuple{
      device.createSwapchainKHRUnique({
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
make_vk_swapchain_image_views(vk::Device device,
                              std::span<const vk::Image> swapchain_images,
                              vk::Format swapchain_image_format) {
  auto retval = std::vector<vk::UniqueImageView>{};
  retval.reserve(swapchain_images.size());
  for (const auto image : swapchain_images) {
    retval.emplace_back(device.createImageViewUnique(
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

vk::UniquePipelineLayout make_vk_pipeline_layout(vk::Device device) {
  return device.createPipelineLayoutUnique({});
}

class Bad_vk_shader_module_size_error : std::exception {};

vk::UniqueShaderModule load_vk_shader_module(vk::Device device,
                                             const char *path) {
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
  return device.createShaderModuleUnique(
      {.codeSize = static_cast<std::uint32_t>(size), .pCode = bytecode.data()});
}

vk::UniquePipeline make_vk_pipeline(vk::Device device,
                                    vk::PipelineLayout layout,
                                    vk::Format swapchain_image_format) {
  const auto vert_shader_module =
      load_vk_shader_module(device, "./assets/shaders/shader.vert.spv");
  const auto frag_shader_module =
      load_vk_shader_module(device, "./assets/shaders/shader.frag.spv");
  const auto shader_stages = std::vector<vk::PipelineShaderStageCreateInfo>{
      {.stage = vk::ShaderStageFlagBits::eVertex,
       .module = *vert_shader_module,
       .pName = "main"},
      {.stage = vk::ShaderStageFlagBits::eFragment,
       .module = *frag_shader_module,
       .pName = "main"}};
  const auto vertex_binding = vk::VertexInputBindingDescription{
      .binding = 0,
      .stride = 5 * sizeof(float),
      .inputRate = vk::VertexInputRate::eVertex};
  const auto vertex_attributes =
      std::vector<vk::VertexInputAttributeDescription>{
          {.location = 0,
           .binding = 0,
           .format = vk::Format::eR32G32Sfloat,
           .offset = 0},
          {.location = 1,
           .binding = 0,
           .format = vk::Format::eR32G32B32Sfloat,
           .offset = 2 * sizeof(float)}};
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
      .pColorAttachmentFormats = &swapchain_image_format};
  return std::move(device
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
                                   .layout = layout}})
                       .value[0]);
}

void record_vk_command_buffer(vk::CommandBuffer command_buffer,
                              vk::Image swapchain_image,
                              vk::ImageView swapchain_image_view,
                              const vk::Extent2D &swapchain_image_extent,
                              vk::Pipeline pipeline, vk::Buffer vertex_buffer,
                              std::size_t vertex_count) {
  command_buffer.begin(vk::CommandBufferBeginInfo{
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
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  command_buffer.pipelineBarrier2(
      {.imageMemoryBarrierCount = 1,
       .pImageMemoryBarriers = &swapchain_image_barrier_1});
  const auto color_attachment = vk::RenderingAttachmentInfo{
      .imageView = swapchain_image_view,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = {{1.0f, 0.0f, 1.0f, 1.0f}}};
  command_buffer.beginRendering(
      {.renderArea = {.offset = {0, 0}, .extent = swapchain_image_extent},
       .layerCount = 1,
       .colorAttachmentCount = 1,
       .pColorAttachments = &color_attachment});
  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
  command_buffer.setViewport(
      0, {{
             .width = static_cast<float>(swapchain_image_extent.width),
             .height = static_cast<float>(swapchain_image_extent.height),
             .minDepth = 0.0f,
             .maxDepth = 1.0f,
         }});
  command_buffer.setScissor(0, {{.extent = swapchain_image_extent}});
  command_buffer.setCullMode(vk::CullModeFlagBits::eNone);
  command_buffer.setFrontFace(vk::FrontFace::eClockwise);
  command_buffer.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
  command_buffer.bindVertexBuffers(0, {vertex_buffer}, {0});
  command_buffer.draw(vertex_count, 1, 0, 0);
  command_buffer.endRendering();
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
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  command_buffer.pipelineBarrier2(
      {.imageMemoryBarrierCount = 1,
       .pImageMemoryBarriers = &swapchain_image_barrier_2});
  command_buffer.end();
}

class Client : public net::Client {
public:
  struct Create_info {
    std::uint32_t incoming_bandwidth{};
    std::uint32_t outgoing_bandwidth{};
    game::Replicated_game game;
  };

  explicit Client(const Create_info &create_info)
      : net::Client{{.incoming_bandwidth = create_info.incoming_bandwidth,
                     .outgoing_bandwidth = create_info.outgoing_bandwidth}},
        _game{create_info.game} {}

protected:
  void on_game_state(serial::Reader &reader) override {
    _game.update(reader);
    const auto players = _game.get_players();
    std::cout << "Player count: " << players.size() << ".\n";
    for (const auto &player : players) {
      const auto &player_position = player.get_position();
      std::cout << "Player position: (" << player_position.x() << ", "
                << player_position.y() << ").\n";
    }
  }

private:
  game::Replicated_game _game;
};
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto glfw_guard = glfw::Initialization_guard{{}};
  const auto glfw_window = glfw::create_window_unique({
      .width = 1920,
      .height = 1080,
      .title = "FPS Party",
      .resizable = true,
      .client_api = glfw::Client_api::no_api,
  });
  std::cout << "Opened window.\n";
  const auto vk_instance = make_vk_instance();
  std::cout << "Created VkInstance.\n";
  const auto vk_surface =
      glfw::create_window_surface_unique(*vk_instance, *glfw_window);
  std::cout << "Created VkSurfaceKHR.\n";
  const auto [vk_physical_device, vk_queue_family_index] =
      find_vk_physical_device(*vk_instance, *vk_surface);
  const auto [vk_device, vk_queue] =
      make_vk_device(vk_physical_device, vk_queue_family_index);
  std::cout << "Created VkDevice.\n";
  const auto vma_allocator =
      make_vma_allocator(*vk_instance, vk_physical_device, *vk_device);
  std::cout << "Created VmaAllocator.\n";
  const auto vk_command_pool = vk_device->createCommandPoolUnique(
      {.flags = vk::CommandPoolCreateFlagBits::eTransient,
       .queueFamilyIndex = vk_queue_family_index});
  std::cout << "Created VkCommandPool.\n";
  auto const triangle_vertices = std::vector<Vertex>{
      {.x = 0.0f, .y = -0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f},
      {.x = -0.5f, .y = 0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f},
      {.x = 0.5f, .y = 0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f}};
  auto const triangle_vertex_buffer =
      upload_vertex_buffer(*vma_allocator, *vk_device, vk_queue,
                           *vk_command_pool, triangle_vertices);
  std::cout << "Uploaded triangle vertex buffer.\n";
  const auto vk_image_acquire_semaphore = vk_device->createSemaphoreUnique({});
  vk_device->setDebugUtilsObjectNameEXT(
      {.objectType = vk::ObjectType::eSemaphore,
       .objectHandle =
           std::bit_cast<std::uint64_t>(*vk_image_acquire_semaphore),
       .pObjectName = "vk_image_acquire_semaphore"});
  const auto vk_image_release_semaphore = vk_device->createSemaphoreUnique({});
  vk_device->setDebugUtilsObjectNameEXT(
      {.objectType = vk::ObjectType::eSemaphore,
       .objectHandle =
           std::bit_cast<std::uint64_t>(*vk_image_release_semaphore),
       .pObjectName = "vk_image_release_semaphore"});
  const auto vk_work_done_fence = vk_device->createFenceUnique(
      {.flags = vk::FenceCreateFlagBits::eSignaled});
  std::cout << "Created per-frame Vulkan synchronization primitives.\n";
  const auto vk_command_buffer =
      std::move(vk_device->allocateCommandBuffersUnique(
          {.commandPool = *vk_command_pool,
           .level = vk::CommandBufferLevel::ePrimary,
           .commandBufferCount = 1})[0]);
  std::cout << "Allocated per-frame VkCommandBuffer.\n";
  auto [vk_swapchain, vk_swapchain_image_format, vk_swapchain_image_extent] =
      make_vk_swapchain(vk_physical_device, *vk_device, *glfw_window,
                        *vk_surface);
  std::cout << "Created VkSwapchainKHR.\n";
  auto vk_swapchain_images = vk_device->getSwapchainImagesKHR(*vk_swapchain);
  std::cout << "Got Vulkan swapchain images.\n";
  auto vk_swapchain_image_views = make_vk_swapchain_image_views(
      *vk_device,
      std::span{vk_swapchain_images.data(), vk_swapchain_images.size()},
      vk_swapchain_image_format);
  std::cout << "Created Vulkan swapchain image views.\n";
  const auto vk_pipeline_layout = make_vk_pipeline_layout(*vk_device);
  std::cout << "Created VkPipelineLayout.\n";
  auto vk_pipeline = make_vk_pipeline(*vk_device, *vk_pipeline_layout,
                                      vk_swapchain_image_format);
  std::cout << "Created VkPipeline.\n";
  const auto enet_guard = enet::Initialization_guard{{}};
  auto game = game::create_replicated_game_unique({});
  auto client = Client{{.game = *game}};
  client.connect({.host = *enet::parse_ip(server_ip), .port = constants::port});
  std::cout << "Connecting to " << server_ip << " on port " << constants::port
            << ".\n";
  while (client.is_connecting()) {
    client.wait_events(100);
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
  while (!signal_status && !glfw_window->should_close() &&
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
          .move_left =
              glfw_window->get_key(glfw::Key::k_a) == glfw::Key_state::press,
          .move_right =
              glfw_window->get_key(glfw::Key::k_d) == glfw::Key_state::press,
          .move_forward =
              glfw_window->get_key(glfw::Key::k_w) == glfw::Key_state::press,
          .move_backward =
              glfw_window->get_key(glfw::Key::k_s) == glfw::Key_state::press,
      });
    }
    client.poll_events();
    try {
      const auto vk_swapchain_image_index =
          vk_device
              ->acquireNextImageKHR(*vk_swapchain,
                                    std::numeric_limits<std::uint64_t>::max(),
                                    *vk_image_acquire_semaphore, {})
              .value;
      std::ignore =
          vk_device->waitForFences(1, &*vk_work_done_fence, true,
                                   std::numeric_limits<std::uint64_t>::max());
      std::ignore = vk_device->resetFences(1, &*vk_work_done_fence);
      vk_device->resetCommandPool(*vk_command_pool);
      record_vk_command_buffer(
          *vk_command_buffer, vk_swapchain_images[vk_swapchain_image_index],
          *vk_swapchain_image_views[vk_swapchain_image_index],
          vk_swapchain_image_extent, *vk_pipeline,
          triangle_vertex_buffer.get_buffer(), triangle_vertices.size());
      const auto vk_image_acquire_wait_stage =
          vk::PipelineStageFlags{vk::PipelineStageFlagBits::eTopOfPipe};
      vk_queue.submit({{.waitSemaphoreCount = 1,
                        .pWaitSemaphores = &*vk_image_acquire_semaphore,
                        .pWaitDstStageMask = &vk_image_acquire_wait_stage,
                        .commandBufferCount = 1,
                        .pCommandBuffers = &*vk_command_buffer,
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores = &*vk_image_release_semaphore}},
                      *vk_work_done_fence);
      const auto vk_queue_present_result =
          vk_queue.presentKHR({.waitSemaphoreCount = 1,
                               .pWaitSemaphores = &*vk_image_release_semaphore,
                               .swapchainCount = 1,
                               .pSwapchains = &*vk_swapchain,
                               .pImageIndices = &vk_swapchain_image_index});
      if (vk_queue_present_result == vk::Result::eSuboptimalKHR) {
        throw vk::OutOfDateKHRError{"Subobtimal queue present result"};
      }
    } catch (const vk::OutOfDateKHRError &e) {
      vk_device->waitIdle();
      vk_swapchain_image_views.clear();
      vk_swapchain_images.clear();
      vk_swapchain.reset();
      const auto previous_vk_swapchain_image_format = vk_swapchain_image_format;
      std::tie(vk_swapchain, vk_swapchain_image_format,
               vk_swapchain_image_extent) =
          make_vk_swapchain(vk_physical_device, *vk_device, *glfw_window,
                            *vk_surface);
      vk_swapchain_images = vk_device->getSwapchainImagesKHR(*vk_swapchain);
      vk_swapchain_image_views = make_vk_swapchain_image_views(
          *vk_device, vk_swapchain_images, vk_swapchain_image_format);
      if (vk_swapchain_image_format != previous_vk_swapchain_image_format) {
        std::cout << "Swapchain changed image format.\n";
        vk_pipeline = make_vk_pipeline(*vk_device, *vk_pipeline_layout,
                                       vk_swapchain_image_format);
      }
    }
  }
  vk_device->waitIdle();
  std::cout << "Exiting.\n";
  return 0;
}
