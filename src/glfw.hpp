#ifndef FPSPARTY_GLFW_HPP
#define FPSPARTY_GLFW_HPP

#ifdef GLFW_INCLUDE_VULKAN
#include <cstring>
#include <span>
#include <vulkan/vulkan.hpp>
#endif

#include <GLFW/glfw3.h>
#include <exception>
#include <string>
#include <utility>

namespace fpsparty::glfw {
#ifdef GLFW_INCLUDE_VULKAN
inline void init_vulkan_loader(PFN_vkGetInstanceProcAddr loader) {
  glfwInitVulkanLoader(loader);
}

inline std::span<const char *> get_required_instance_extensions() {
  auto extension_count = std::uint32_t{};
  const auto extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  return std::span{extensions, extension_count};
}

inline GLFWvkproc get_instance_proc_address(vk::Instance instance,
                                            const char *procname) {
  return glfwGetInstanceProcAddress(instance, procname);
}

inline int get_physical_device_presentation_support(vk::Instance instance,
                                                    vk::PhysicalDevice device,
                                                    std::uint32_t queuefamily) {
  return glfwGetPhysicalDevicePresentationSupport(instance, device,
                                                  queuefamily);
}

inline vk::SurfaceKHR
create_window_surface(vk::Instance instance, GLFWwindow *window,
                      const vk::AllocationCallbacks *allocator = nullptr) {
  auto native_allocator = VkAllocationCallbacks{};
  if (allocator) {
    std::memcpy(&native_allocator, allocator, sizeof(VkAllocationCallbacks));
  }
  auto surface = VkSurfaceKHR{};
  std::ignore = glfwCreateWindowSurface(
      instance, window, allocator ? &native_allocator : nullptr, &surface);
  return surface;
}

inline vk::UniqueSurfaceKHR create_window_surface_unique(
    vk::Instance instance, GLFWwindow *window,
    const vk::AllocationCallbacks *allocator = nullptr) {
  return vk::UniqueSurfaceKHR{
      create_window_surface(instance, window, allocator),
      instance,
  };
}
#endif

class Error : std::exception {
public:
  explicit Error(int error_code, const char *description)
      : _error_code{error_code}, _description{description} {}

  constexpr int get_error_code() const noexcept { return _error_code; }

  constexpr const std::string &get_description() const noexcept {
    return _description;
  }

private:
  int _error_code;
  std::string _description;
};

class Initialization_guard {
public:
  struct Create_info {};

  constexpr Initialization_guard() noexcept = default;

  explicit Initialization_guard(Create_info) : _owning{true} {
    glfwSetErrorCallback([](int error_code, const char *description) {
      throw Error{error_code, description};
    });
    glfwInit();
  }

  constexpr Initialization_guard(Initialization_guard &&other) noexcept
      : _owning{std::exchange(other._owning, false)} {}

  Initialization_guard &operator=(Initialization_guard &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Initialization_guard() {
    if (_owning) {
      glfwTerminate();
    }
  }

private:
  constexpr void swap(Initialization_guard &other) noexcept {
    std::swap(_owning, other._owning);
  }

  bool _owning{};
};

enum class Client_api {
  opengl_api = GLFW_OPENGL_API,
  opengl_es_api = GLFW_OPENGL_ES_API,
  no_api = GLFW_NO_API,
};

enum class Context_creation_api {
  native_context_api = GLFW_NATIVE_CONTEXT_API,
  egl_context_api = GLFW_EGL_CONTEXT_API,
  osmesa_context_api = GLFW_OSMESA_CONTEXT_API,
};

enum class Context_robustness {
  no_robustness = GLFW_NO_ROBUSTNESS,
  no_reset_notification = GLFW_NO_RESET_NOTIFICATION,
  lose_context_on_reset = GLFW_LOSE_CONTEXT_ON_RESET,
};

enum class Context_release_behavior {
  any = GLFW_ANY_RELEASE_BEHAVIOR,
  flush = GLFW_RELEASE_BEHAVIOR_FLUSH,
  none = GLFW_RELEASE_BEHAVIOR_NONE
};

enum class Opengl_profile {
  any = GLFW_OPENGL_ANY_PROFILE,
  compat = GLFW_OPENGL_COMPAT_PROFILE,
  core = GLFW_OPENGL_CORE_PROFILE
};

class Window {
public:
  struct Create_info {
    int width;
    int height;
    const char *title;
    bool resizable{true};
    bool visible{true};
    bool decorated{true};
    bool focused{true};
    bool auto_iconify{true};
    bool floating{false};
    bool maximized{false};
    bool center_cursor{true};
    bool transparent_framebuffer{false};
    bool focus_on_show{true};
    bool scale_to_monitor{false};
    bool scale_framebuffer{true};
    bool mouse_passthrough{false};
    int position_x{static_cast<int>(GLFW_ANY_POSITION)};
    int position_y{static_cast<int>(GLFW_ANY_POSITION)};
    int red_bits{8};
    int green_bits{8};
    int blue_bits{8};
    int alpha_bits{8};
    int depth_bits{24};
    int stencil_bits{8};
    int accum_red_bits{0};
    int accum_green_bits{0};
    int accum_blue_bits{0};
    int accum_alpha_bits{0};
    int aux_buffers{0};
    int samples{0};
    int refresh_rate{GLFW_DONT_CARE};
    bool stereo{false};
    bool srgb_capable{false};
    bool doublebuffer{true};
    Client_api client_api{Client_api::opengl_api};
    Context_creation_api context_creation_api{
        Context_creation_api::native_context_api};
    int context_version_major{1};
    int context_version_minor{1};
    Context_robustness context_robustness{Context_robustness::no_robustness};
    Context_release_behavior context_release_behavior{
        Context_release_behavior::any};
    bool opengl_forward_compat{false};
    bool context_debug{false};
    Opengl_profile opengl_profile{Opengl_profile::any};
    bool win32_keyboard_menu{false};
    bool win32_showdefault{false};
    const char *cocoa_frame_name{""};
    bool cocoa_graphics_switching{false};
    const char *wayland_app_id{""};
    const char *x11_class_name{""};
    const char *x11_instance_name{""};
  };

  constexpr Window() noexcept = default;

  explicit Window(const Create_info &create_info) {
    glfwWindowHint(GLFW_RESIZABLE, create_info.resizable);
    glfwWindowHint(GLFW_VISIBLE, create_info.visible);
    glfwWindowHint(GLFW_DECORATED, create_info.decorated);
    glfwWindowHint(GLFW_FOCUSED, create_info.focused);
    glfwWindowHint(GLFW_AUTO_ICONIFY, create_info.auto_iconify);
    glfwWindowHint(GLFW_FLOATING, create_info.floating);
    glfwWindowHint(GLFW_MAXIMIZED, create_info.maximized);
    glfwWindowHint(GLFW_CENTER_CURSOR, create_info.center_cursor);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER,
                   create_info.transparent_framebuffer);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, create_info.focus_on_show);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, create_info.scale_to_monitor);
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, create_info.scale_framebuffer);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, create_info.mouse_passthrough);
    glfwWindowHint(GLFW_POSITION_X, create_info.position_x);
    glfwWindowHint(GLFW_POSITION_Y, create_info.position_y);
    glfwWindowHint(GLFW_RED_BITS, create_info.red_bits);
    glfwWindowHint(GLFW_GREEN_BITS, create_info.green_bits);
    glfwWindowHint(GLFW_BLUE_BITS, create_info.blue_bits);
    glfwWindowHint(GLFW_ALPHA_BITS, create_info.alpha_bits);
    glfwWindowHint(GLFW_DEPTH_BITS, create_info.depth_bits);
    glfwWindowHint(GLFW_STENCIL_BITS, create_info.stencil_bits);
    glfwWindowHint(GLFW_ACCUM_RED_BITS, create_info.accum_red_bits);
    glfwWindowHint(GLFW_ACCUM_GREEN_BITS, create_info.accum_green_bits);
    glfwWindowHint(GLFW_ACCUM_BLUE_BITS, create_info.accum_blue_bits);
    glfwWindowHint(GLFW_ACCUM_ALPHA_BITS, create_info.accum_alpha_bits);
    glfwWindowHint(GLFW_AUX_BUFFERS, create_info.aux_buffers);
    glfwWindowHint(GLFW_SAMPLES, create_info.samples);
    glfwWindowHint(GLFW_REFRESH_RATE, create_info.refresh_rate);
    glfwWindowHint(GLFW_STEREO, create_info.stereo);
    glfwWindowHint(GLFW_SRGB_CAPABLE, create_info.srgb_capable);
    glfwWindowHint(GLFW_DOUBLEBUFFER, create_info.doublebuffer);
    glfwWindowHint(GLFW_CLIENT_API, static_cast<int>(create_info.client_api));
    glfwWindowHint(GLFW_CONTEXT_CREATION_API,
                   static_cast<int>(create_info.context_creation_api));
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,
                   create_info.context_version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,
                   create_info.context_version_minor);
    glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS,
                   static_cast<int>(create_info.context_robustness));
    glfwWindowHint(GLFW_CONTEXT_RELEASE_BEHAVIOR,
                   static_cast<int>(create_info.context_release_behavior));
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,
                   create_info.opengl_forward_compat);
    glfwWindowHint(GLFW_CONTEXT_DEBUG, create_info.context_debug);
    glfwWindowHint(GLFW_OPENGL_PROFILE,
                   static_cast<int>(create_info.opengl_profile));
    glfwWindowHint(GLFW_WIN32_KEYBOARD_MENU, create_info.win32_keyboard_menu);
    glfwWindowHint(GLFW_WIN32_SHOWDEFAULT, create_info.win32_showdefault);
    glfwWindowHintString(GLFW_COCOA_FRAME_NAME, create_info.cocoa_frame_name);
    glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING,
                   create_info.cocoa_graphics_switching);
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, create_info.wayland_app_id);
    glfwWindowHintString(GLFW_X11_CLASS_NAME, create_info.x11_class_name);
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, create_info.x11_instance_name);
    _value = glfwCreateWindow(create_info.width, create_info.height,
                              create_info.title, nullptr, nullptr);
  }

  constexpr Window(Window &&other) noexcept
      : _value{std::exchange(other._value, nullptr)} {}

  Window &operator=(Window &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Window() {
    if (_value) {
      glfwDestroyWindow(_value);
    }
  }

  constexpr operator GLFWwindow *() const noexcept { return _value; }

  bool should_close() const { return glfwWindowShouldClose(_value); }

private:
  constexpr void swap(Window &other) noexcept {
    std::swap(_value, other._value);
  }

  GLFWwindow *_value{};
};

inline void poll_events() { glfwPollEvents(); }
} // namespace fpsparty::glfw

#endif
