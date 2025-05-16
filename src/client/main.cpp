#include "constants.hpp"
#include "enet.hpp"
#include "glfw.hpp"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_funcs.hpp>

using namespace fpsparty;

namespace {
const auto vulkan_device_extensions =
    std::array{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr auto server_ip = "127.0.0.1";

volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }

vk::UniqueInstance make_vk_instance() {
  const auto app_info = vk::ApplicationInfo{
      .pApplicationName = "FPS Party",
      .pEngineName = "FPS Party",
      .apiVersion = VK_API_VERSION_1_3,
  };
#ifndef NDEBUG
  std::cout << "Enabling Vulkan validation layers.\n";
  const auto layers = std::array{"VK_LAYER_KHRONOS_validation"};
#endif
  const auto extensions = glfw::get_required_instance_extensions();
  const auto create_info = vk::InstanceCreateInfo{
      .pApplicationInfo = &app_info,
#ifndef NDEBUG
      .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
#endif
      .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
  };
  return vk::createInstanceUnique(create_info);
}

/**
 * @return a tuple with a physical device and a queue family index
 */
std::tuple<vk::PhysicalDevice, std::uint32_t>
find_vk_physical_device(vk::Instance instance, vk::SurfaceKHR surface) {
  const auto physical_devices = instance.enumeratePhysicalDevices();
  for (const auto physical_device : physical_devices) {
    const auto properties = physical_device.getProperties();
    if (properties.apiVersion < VK_API_VERSION_1_3) {
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
      for (const auto extension : vulkan_device_extensions) {
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
          static_cast<std::uint32_t>(vulkan_device_extensions.size()),
      .ppEnabledExtensionNames = vulkan_device_extensions.data(),
  });
  const auto queue = device->getQueue(queue_family_index, 0);
  return std::tuple{std::move(device), queue};
}
} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto glfw_guard = glfw::Initialization_guard{{}};
  const auto window = glfw::Window{{
      .width = 1920,
      .height = 1080,
      .title = "FPS Party",
      .resizable = false,
      .client_api = glfw::Client_api::no_api,
  }};
  std::cout << "Opened window.\n";
  const auto vk_instance = make_vk_instance();
  std::cout << "Created VkInstance.\n";
  const auto vk_surface =
      glfw::create_window_surface_unique(*vk_instance, window);
  std::cout << "Created VkSurfaceKHR.\n";
  const auto [vk_physical_device, vk_queue_family_index] =
      find_vk_physical_device(*vk_instance, *vk_surface);
  const auto [vk_device, vk_queue] =
      make_vk_device(vk_physical_device, vk_queue_family_index);
  std::cout << "Created VkDevice.\n";
  const auto enet_guard = enet::Initialization_guard{{}};
  const auto client = enet::make_client_host({
      .max_channel_count = 1,
  });
  client.connect({.host = *enet::parse_ip(server_ip), .port = constants::port},
                 1, 0);
  std::cout << "Connecting to " << server_ip << " on port " << constants::port
            << ".\n";
  const auto connect_event = client.service(5000);
  if (connect_event.type == enet::Event_type::connect) {
    std::cout << "Connection successful.\n";
  } else {
    std::cout << "Connection failed.\n";
    return 0;
  }
  auto connected = true;
  while (!signal_status && !window.should_close() && connected) {
    glfw::poll_events();
    const auto event = client.service(0);
    switch (event.type) {
    case enet::Event_type::disconnect:
      std::cout << "Got disconnect event.\n";
      connected = false;
      continue;
    default:
      break;
    }
  }
  std::cout << "Exiting.\n";
  return 0;
}
