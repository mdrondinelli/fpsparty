#ifndef FPSPARTY_GLFW_HPP
#define FPSPARTY_GLFW_HPP

#ifdef GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.hpp>
#endif

#include <GLFW/glfw3.h>
#include <array>
#include <cstring>
#include <exception>
#include <memory>
#include <span>
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
  none = GLFW_RELEASE_BEHAVIOR_NONE,
};

enum class Opengl_profile {
  any = GLFW_OPENGL_ANY_PROFILE,
  compat = GLFW_OPENGL_COMPAT_PROFILE,
  core = GLFW_OPENGL_CORE_PROFILE,
};

enum class Cursor_input_mode {
  normal = GLFW_CURSOR_NORMAL,
  disabled = GLFW_CURSOR_DISABLED,
  hidden = GLFW_CURSOR_HIDDEN,
};

enum class Key {
  k_space = 32,
  k_apostrophe = 39, /* ' */
  k_comma = 44,      /* , */
  k_minus = 45,      /* - */
  k_period = 46,     /* . */
  k_slash = 47,      /* / */
  k_0 = 48,
  k_1 = 49,
  k_2 = 50,
  k_3 = 51,
  k_4 = 52,
  k_5 = 53,
  k_6 = 54,
  k_7 = 55,
  k_8 = 56,
  k_9 = 57,
  k_semicolon = 59, /* ; */
  k_equal = 61,     /* = */
  k_a = 65,
  k_b = 66,
  k_c = 67,
  k_d = 68,
  k_e = 69,
  k_f = 70,
  k_g = 71,
  k_h = 72,
  k_i = 73,
  k_j = 74,
  k_k = 75,
  k_l = 76,
  k_m = 77,
  k_n = 78,
  k_o = 79,
  k_p = 80,
  k_q = 81,
  k_r = 82,
  k_s = 83,
  k_t = 84,
  k_u = 85,
  k_v = 86,
  k_w = 87,
  k_x = 88,
  k_y = 89,
  k_z = 90,
  k_left_bracket = 91,  /* [ */
  k_backslash = 92,     /* \ */
  k_right_bracket = 93, /* ] */
  k_grave_accent = 96,  /* ` */
  k_world_1 = 161,      /* non-us #1 */
  k_world_2 = 162,      /* non-us #2 */
  k_escape = 256,
  k_enter = 257,
  k_tab = 258,
  k_backspace = 259,
  k_insert = 260,
  k_delete = 261,
  k_right = 262,
  k_left = 263,
  k_down = 264,
  k_up = 265,
  k_page_up = 266,
  k_page_down = 267,
  k_home = 268,
  k_end = 269,
  k_caps_lock = 280,
  k_scroll_lock = 281,
  k_num_lock = 282,
  k_print_screen = 283,
  k_pause = 284,
  k_f1 = 290,
  k_f2 = 291,
  k_f3 = 292,
  k_f4 = 293,
  k_f5 = 294,
  k_f6 = 295,
  k_f7 = 296,
  k_f8 = 297,
  k_f9 = 298,
  k_f10 = 299,
  k_f11 = 300,
  k_f12 = 301,
  k_f13 = 302,
  k_f14 = 303,
  k_f15 = 304,
  k_f16 = 305,
  k_f17 = 306,
  k_f18 = 307,
  k_f19 = 308,
  k_f20 = 309,
  k_f21 = 310,
  k_f22 = 311,
  k_f23 = 312,
  k_f24 = 313,
  k_f25 = 314,
  k_kp_0 = 320,
  k_kp_1 = 321,
  k_kp_2 = 322,
  k_kp_3 = 323,
  k_kp_4 = 324,
  k_kp_5 = 325,
  k_kp_6 = 326,
  k_kp_7 = 327,
  k_kp_8 = 328,
  k_kp_9 = 329,
  k_kp_decimal = 330,
  k_kp_divide = 331,
  k_kp_multiply = 332,
  k_kp_subtract = 333,
  k_kp_add = 334,
  k_kp_enter = 335,
  k_kp_equal = 336,
  k_left_shift = 340,
  k_left_control = 341,
  k_left_alt = 342,
  k_left_super = 343,
  k_right_shift = 344,
  k_right_control = 345,
  k_right_alt = 346,
  k_right_super = 347,
  k_menu = 348,
};

enum class Mouse_button {
  mb_1 = GLFW_MOUSE_BUTTON_1,
  mb_2 = GLFW_MOUSE_BUTTON_2,
  mb_3 = GLFW_MOUSE_BUTTON_3,
  mb_4 = GLFW_MOUSE_BUTTON_4,
  mb_5 = GLFW_MOUSE_BUTTON_5,
  mb_6 = GLFW_MOUSE_BUTTON_6,
  mb_7 = GLFW_MOUSE_BUTTON_7,
  mb_8 = GLFW_MOUSE_BUTTON_8,
  mb_left = mb_1,
  mb_right = mb_2,
  mb_middle = mb_3,
};

enum class Press_state {
  press = GLFW_PRESS,
  release = GLFW_RELEASE,
};

enum class Press_action {
  press = GLFW_PRESS,
  release = GLFW_RELEASE,
  repeat = GLFW_REPEAT,
};

enum class Joystick_event {
  connected = GLFW_CONNECTED,
  disconnected = GLFW_DISCONNECTED,
};

class Window;

class Mouse_button_callback {
public:
  virtual ~Mouse_button_callback() = default;

  virtual void on_mouse_button(Window window, Mouse_button button,
                               Press_state action, int mods) = 0;
};

class Cursor_pos_callback {
public:
  virtual ~Cursor_pos_callback() = default;

  virtual void on_cursor_pos(Window window, double xpos, double ypos,
                             double dxpos, double dypos) = 0;
};

class Cursor_enter_callback {
public:
  virtual ~Cursor_enter_callback() = default;

  virtual void on_cursor_enter(Window window, bool entered) = 0;
};

class Scroll_callback {
public:
  virtual ~Scroll_callback() = default;

  virtual void on_scroll(Window window, double xoffset, double yoffset) = 0;
};

class Key_callback {
public:
  virtual ~Key_callback() = default;

  virtual void on_key(Window window, Key key, int scancode, Press_action action,
                      int mods) = 0;
};

class Char_callback {
public:
  virtual ~Char_callback() = default;

  virtual void on_char(Window window, unsigned int codepoint) = 0;
};

class Char_mods_callback {
public:
  virtual ~Char_mods_callback() = default;

  virtual void on_char_mods(Window window, unsigned int codepoint,
                            int mods) = 0;
};

class Drop_callback {
public:
  virtual ~Drop_callback() = default;

  virtual void on_drop(Window window, std::span<const char *> paths) = 0;
};

class Joystick_callback {
public:
  virtual ~Joystick_callback() = default;

  virtual void on_joystick(int jid, Joystick_event event) = 0;
};

class Window {
  struct User_data {
    Mouse_button_callback *mouse_button_callback{};
    Cursor_pos_callback *cursor_pos_callback{};
    Cursor_enter_callback *cursor_enter_callback{};
    Scroll_callback *scroll_callback{};
    Key_callback *key_callback{};
    Char_callback *char_callback{};
    Char_mods_callback *char_mods_callback{};
    Drop_callback *drop_callback{};
    Joystick_callback *joystick_callback{};
    double cursor_pos_x{};
    double cursor_pos_y{};
    bool cursor_pos_valid{};
    void *next{};
  };

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

  constexpr operator GLFWwindow *() const noexcept { return _value; }

  void *get_user_pointer() const { return get_user_data().next; }

  void set_user_pointer(void *pointer) const { get_user_data().next = pointer; }

  bool should_close() const { return glfwWindowShouldClose(_value); }

  std::array<int, 2> get_framebuffer_size() const {
    auto retval = std::array<int, 2>{};
    glfwGetFramebufferSize(_value, &retval[0], &retval[1]);
    return retval;
  }

  Cursor_input_mode get_cursor_input_mode() const {
    return static_cast<Cursor_input_mode>(
        glfwGetInputMode(_value, GLFW_CURSOR));
  }

  void set_cursor_input_mode(Cursor_input_mode value) const {
    glfwSetInputMode(_value, GLFW_CURSOR, static_cast<int>(value));
  }

  Press_state get_key(Key key) const {
    return static_cast<Press_state>(glfwGetKey(_value, static_cast<int>(key)));
  }

  Press_state get_mouse_button(Mouse_button button) const {
    return static_cast<Press_state>(
        glfwGetMouseButton(_value, static_cast<int>(button)));
  }

  void set_key_callback(Key_callback *callback) const noexcept {
    get_user_data().key_callback = callback;
  }

  void set_char_callback(Char_callback *callback) const noexcept {
    get_user_data().char_callback = callback;
  }

  void set_char_mods_callback(Char_mods_callback *callback) const noexcept {
    get_user_data().char_mods_callback = callback;
  }

  void
  set_mouse_button_callback(Mouse_button_callback *callback) const noexcept {
    get_user_data().mouse_button_callback = callback;
  }

  void set_cursor_pos_callback(Cursor_pos_callback *callback) const noexcept {
    get_user_data().cursor_pos_callback = callback;
  }

  void
  set_cursor_enter_callback(Cursor_enter_callback *callback) const noexcept {
    get_user_data().cursor_enter_callback = callback;
  }

  void set_scroll_callback(Scroll_callback *callback) const noexcept {
    get_user_data().scroll_callback = callback;
  }

  void set_drop_callback(Drop_callback *callback) const noexcept {
    get_user_data().drop_callback = callback;
  }

  void set_joystick_callback(Joystick_callback *callback) const noexcept {
    get_user_data().joystick_callback = callback;
  }

private:
  friend Window create_window(const Window::Create_info &create_info);

  constexpr Window(GLFWwindow *value) noexcept : _value{value} {}

  User_data &get_user_data() const noexcept {
    return *static_cast<User_data *>(glfwGetWindowUserPointer(_value));
  }

  GLFWwindow *_value{};
};

inline Window create_window(const Window::Create_info &create_info) {
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
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, create_info.context_version_major);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, create_info.context_version_minor);
  glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS,
                 static_cast<int>(create_info.context_robustness));
  glfwWindowHint(GLFW_CONTEXT_RELEASE_BEHAVIOR,
                 static_cast<int>(create_info.context_release_behavior));
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, create_info.opengl_forward_compat);
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
  auto user_data = std::make_unique<Window::User_data>();
  const auto window = glfwCreateWindow(create_info.width, create_info.height,
                                       create_info.title, nullptr, nullptr);
  glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode,
                                int action, int mods) {
    const auto user_data =
        static_cast<Window::User_data *>(glfwGetWindowUserPointer(window));
    if (user_data->key_callback) {
      user_data->key_callback->on_key(window, static_cast<Key>(key), scancode,
                                      static_cast<Press_action>(action), mods);
    }
  });
  glfwSetCharCallback(window, [](GLFWwindow *window, unsigned int codepoint) {
    const auto user_data =
        static_cast<Window::User_data *>(glfwGetWindowUserPointer(window));
    if (user_data->char_callback) {
      user_data->char_callback->on_char(window, codepoint);
    }
  });
  glfwSetCharModsCallback(
      window, [](GLFWwindow *window, unsigned int codepoint, int mods) {
        const auto user_data =
            static_cast<Window::User_data *>(glfwGetWindowUserPointer(window));
        if (user_data->char_mods_callback) {
          user_data->char_mods_callback->on_char_mods(window, codepoint, mods);
        }
      });
  glfwSetMouseButtonCallback(
      window, [](GLFWwindow *window, int button, int action, int mods) {
        const auto user_data =
            static_cast<Window::User_data *>(glfwGetWindowUserPointer(window));
        if (user_data->mouse_button_callback) {
          user_data->mouse_button_callback->on_mouse_button(
              window, static_cast<Mouse_button>(button),
              static_cast<Press_state>(action), mods);
        }
      });
  glfwSetCursorPosCallback(
      window, [](GLFWwindow *window, double xpos, double ypos) {
        const auto user_data =
            static_cast<Window::User_data *>(glfwGetWindowUserPointer(window));
        if (user_data->cursor_pos_callback) {
          auto dxpos = 0.0;
          auto dypos = 0.0;
          if (user_data->cursor_pos_valid) {
            dxpos = xpos - user_data->cursor_pos_x;
            dypos = ypos - user_data->cursor_pos_y;
          }
          user_data->cursor_pos_callback->on_cursor_pos(window, xpos, ypos,
                                                        dxpos, dypos);
          user_data->cursor_pos_x = xpos;
          user_data->cursor_pos_y = ypos;
          user_data->cursor_pos_valid = true;
        }
      });
  glfwSetCursorEnterCallback(window, [](GLFWwindow *window, int entered) {
    const auto user_data =
        *static_cast<Window::User_data *>(glfwGetWindowUserPointer(window));
    if (user_data.cursor_enter_callback) {
      user_data.cursor_enter_callback->on_cursor_enter(window, entered);
    }
  });
  glfwSetScrollCallback(
      window, [](GLFWwindow *window, double xoffset, double yoffset) {
        const auto user_data =
            *static_cast<Window::User_data *>(glfwGetWindowUserPointer(window));
        if (user_data.scroll_callback) {
          user_data.scroll_callback->on_scroll(window, xoffset, yoffset);
        }
      });
  glfwSetDropCallback(window, [](GLFWwindow *window, int path_count,
                                 const char **paths) {
    const auto user_data =
        *static_cast<Window::User_data *>(glfwGetWindowUserPointer(window));
    if (user_data.drop_callback) {
      user_data.drop_callback->on_drop(window,
                                       std::span{paths, paths + path_count});
    }
  });
  glfwSetWindowUserPointer(window, user_data.release());
  return window;
}

inline void destroy_window(Window window) { glfwDestroyWindow(window); }

class Unique_window {
public:
  constexpr Unique_window() noexcept = default;

  constexpr explicit Unique_window(Window value) noexcept : _value{value} {}

  constexpr Unique_window(Unique_window &&other) noexcept
      : _value{std::exchange(other._value, Window{})} {}

  Unique_window &operator=(Unique_window &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Unique_window() {
    if (_value) {
      destroy_window(_value);
    }
  }

  constexpr Window operator*() const noexcept { return _value; }

  constexpr const Window *operator->() const noexcept { return &_value; }

private:
  constexpr void swap(Unique_window &other) noexcept {
    std::swap(_value, other._value);
  }

  Window _value{};
};

inline Unique_window
create_window_unique(const Window::Create_info &create_info) {
  return Unique_window{create_window(create_info)};
}

inline void poll_events() { glfwPollEvents(); }

} // namespace fpsparty::glfw

#endif
