#ifndef FPSPARTY_CLIENT_GLOBAL_VULKAN_STATE_HPP
#define FPSPARTY_CLIENT_GLOBAL_VULKAN_STATE_HPP

#include <memory>
#include <mutex>
#include <vulkan/vulkan.hpp>

namespace fpsparty::client {
class Global_vulkan_state {
public:
  static const Global_vulkan_state &get() noexcept;

  vk::Instance instance() const noexcept { return *_instance; }

  vk::PhysicalDevice physical_device() const noexcept {
    return _physical_device;
  }

  std::uint32_t queue_family_index() const noexcept {
    return _queue_family_index;
  }

  vk::Device device() const noexcept { return *_device; }

  vk::Queue queue() const noexcept { return _queue; }

private:
  friend class Global_vulkan_state_guard;

  static void add_reference();

  static void remove_reference() noexcept;

  static std::unique_ptr<Global_vulkan_state> _global_instance;
  static unsigned int _global_instance_refcount;
  static std::mutex _global_instance_mutex;

  explicit Global_vulkan_state();

  vk::UniqueInstance _instance{};
  vk::PhysicalDevice _physical_device{};
  std::uint32_t _queue_family_index{};
  vk::UniqueDevice _device{};
  vk::Queue _queue{};
};

class Global_vulkan_state_guard {
public:
  struct Create_info {};

  constexpr Global_vulkan_state_guard() noexcept = default;

  explicit Global_vulkan_state_guard(const Create_info &info);

  ~Global_vulkan_state_guard();

private:
  bool _owning{};
};
} // namespace fpsparty::client

#endif
