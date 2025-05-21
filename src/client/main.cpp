#include "constants.hpp"
#include "enet.hpp"
#include "glfw.hpp"
#include <algorithm>
#include <bit>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <tuple>
#include <volk.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_funcs.hpp>
#include <vulkan/vulkan_handles.hpp>

using namespace fpsparty;

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

void remake_vk_swapchain_and_image_views(
    vk::PhysicalDevice physical_device, vk::Device device, glfw::Window window,
    vk::SurfaceKHR surface, vk::UniqueSwapchainKHR &swapchain,
    vk::Format &swapchain_image_format, vk::Extent2D &swapchain_image_extent,
    std::vector<vk::Image> &swapchain_images,
    std::vector<vk::UniqueImageView> &swapchain_image_views) {
  device.waitIdle();
  swapchain_image_views.clear();
  swapchain_images.clear();
  swapchain.reset();
  std::tie(swapchain, swapchain_image_format, swapchain_image_extent) =
      make_vk_swapchain(physical_device, device, window, surface);
  swapchain_images = device.getSwapchainImagesKHR(*swapchain);
  swapchain_image_views = make_vk_swapchain_image_views(
      device, swapchain_images, swapchain_image_format);
}

void record_vk_command_buffer(vk::CommandBuffer command_buffer,
                              vk::Image swapchain_image,
                              vk::ImageView swapchain_image_view,
                              const vk::Extent2D &swapchain_image_extent) {
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
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto glfw_guard = glfw::Initialization_guard{{}};
  const auto window = glfw::create_window_unique({
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
      glfw::create_window_surface_unique(*vk_instance, *window);
  std::cout << "Created VkSurfaceKHR.\n";
  const auto [vk_physical_device, vk_queue_family_index] =
      find_vk_physical_device(*vk_instance, *vk_surface);
  const auto [vk_device, vk_queue] =
      make_vk_device(vk_physical_device, vk_queue_family_index);
  std::cout << "Created VkDevice.\n";
  auto [vk_swapchain, vk_swapchain_image_format, vk_swapchain_image_extent] =
      make_vk_swapchain(vk_physical_device, *vk_device, *window, *vk_surface);
  std::cout << "Created VkSwapchainKHR.\n";
  auto vk_swapchain_images = vk_device->getSwapchainImagesKHR(*vk_swapchain);
  std::cout << "Got Vulkan swapchain images.\n";
  auto vk_swapchain_image_views = make_vk_swapchain_image_views(
      *vk_device,
      std::span{vk_swapchain_images.data(), vk_swapchain_images.size()},
      vk_swapchain_image_format);
  std::cout << "Created Vulkan swapchain image views.\n";
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
  const auto vk_command_pool = vk_device->createCommandPoolUnique(
      {.flags = vk::CommandPoolCreateFlagBits::eTransient,
       .queueFamilyIndex = vk_queue_family_index});
  const auto vk_command_buffer =
      std::move(vk_device->allocateCommandBuffersUnique(
          {.commandPool = *vk_command_pool,
           .level = vk::CommandBufferLevel::ePrimary,
           .commandBufferCount = 1})[0]);
  const auto enet_guard = enet::Initialization_guard{{}};
  const auto client = enet::make_client_host_unique({.max_channel_count = 1});
  client->connect({.host = *enet::parse_ip(server_ip), .port = constants::port},
                  1, 0);
  std::cout << "Connecting to " << server_ip << " on port " << constants::port
            << ".\n";
  const auto connect_event = client->service(5000);
  if (connect_event.type == enet::Event_type::connect) {
    std::cout << "Connection successful.\n";
  } else {
    std::cout << "Connection failed.\n";
    return 0;
  }
  auto connected = true;
  while (!signal_status && !window->should_close() && connected) {
    glfw::poll_events();
    for (;;) {
      const auto event = client->service(0);
      switch (event.type) {
      default:
        continue;
      case enet::Event_type::disconnect:
        std::cout << "Got disconnect event.\n";
        connected = false;
        continue;
      case enet::Event_type::none:
        goto after_enet_event_loop;
      }
    }
  after_enet_event_loop:
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
          vk_swapchain_image_extent);
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
      remake_vk_swapchain_and_image_views(
          vk_physical_device, *vk_device, *window, *vk_surface, vk_swapchain,
          vk_swapchain_image_format, vk_swapchain_image_extent,
          vk_swapchain_images, vk_swapchain_image_views);
    }
  }
  vk_device->waitIdle();
  std::cout << "Exiting.\n";
  return 0;
}
