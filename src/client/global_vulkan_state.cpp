#include "global_vulkan_state.hpp"
#include "glfw.hpp"
#include <iostream>
#include <memory>
#include <mutex>
#include <volk.h>
#include <vulkan/vulkan_handles.hpp>

namespace fpsparty::client {
namespace {
const auto vk_device_extensions = std::array{vk::KHRSwapchainExtensionName};

vk::UniqueInstance make_vk_instance() {
  volkInitialize();
  const auto app_info = vk::ApplicationInfo{
      .pApplicationName = "FPS Party",
      .pEngineName = "FPS Party",
      .apiVersion = vk::ApiVersion13,
  };
#ifndef FPSPARTY_VULKAN_NDEBUG
  std::cout << "Enabling Vulkan validation layers.\n";
  const auto layers = std::array{"VK_LAYER_KHRONOS_validation"};
#endif
#ifndef FPSPARTY_VULKAN_NDEBUG
  const auto glfw_extensions = glfw::get_required_instance_extensions();
  std::cout << "Enabling Vulkan debug extension.\n";
  auto extensions = std::vector<const char *>(std::from_range, glfw_extensions);
  extensions.push_back(vk::EXTDebugUtilsExtensionName);
#else
  const auto extensions = glfw::get_required_instance_extensions();
#endif
  const auto create_info = vk::InstanceCreateInfo{
      .pApplicationInfo = &app_info,
#ifndef FPSPARTY_VULKAN_NDEBUG
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

/**
 * @return a tuple with a physical device and a queue family index
 */
std::tuple<vk::PhysicalDevice, std::uint32_t>
find_vk_physical_device(vk::Instance instance) {
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
            glfw::get_physical_device_presentation_support(
                instance, physical_device, i)) {
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
                << "' because it does not have required extensions.\n";
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
  std::cout << "Created VkDevice.\n";
  return std::tuple{std::move(device), queue};
}
} // namespace

std::unique_ptr<Global_vulkan_state> Global_vulkan_state::_global_instance{};
unsigned int Global_vulkan_state::_global_instance_refcount{};
std::mutex Global_vulkan_state::_global_instance_mutex{};

const Global_vulkan_state &Global_vulkan_state::get() noexcept {
  return *_global_instance;
}

void Global_vulkan_state::add_reference() {
  const auto lock = std::scoped_lock{_global_instance_mutex};
  const auto refcount = _global_instance_refcount++;
  if (refcount == 0) {
    _global_instance =
        std::unique_ptr<Global_vulkan_state>{new Global_vulkan_state};
  }
}

void Global_vulkan_state::remove_reference() noexcept {
  const auto lock = std::scoped_lock{_global_instance_mutex};
  const auto refcount = --_global_instance_refcount;
  if (refcount == 0) {
    _global_instance.reset();
  }
}

Global_vulkan_state::Global_vulkan_state() {
  _instance = make_vk_instance();
  std::tie(_physical_device, _queue_family_index) =
      find_vk_physical_device(*_instance);
  std::tie(_device, _queue) =
      make_vk_device(_physical_device, _queue_family_index);
}

Global_vulkan_state_guard::Global_vulkan_state_guard(const Create_info &)
    : _owning{true} {
  Global_vulkan_state::add_reference();
}

Global_vulkan_state_guard::~Global_vulkan_state_guard() {
  if (_owning) {
    Global_vulkan_state::remove_reference();
  }
}
} // namespace fpsparty::client
