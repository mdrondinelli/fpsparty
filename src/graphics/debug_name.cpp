#include "graphics/debug_name.hpp"
#include "graphics/global_vulkan_state.hpp"

namespace fpsparty::graphics::detail {

void set_debug_name(
  [[maybe_unused]] vk::ObjectType object_type,
  [[maybe_unused]] u64 object_handle,
  [[maybe_unused]] Debug_name name) noexcept {
#ifdef FPSPARTY_VULKAN_DEBUG_NAMES
  if (name.c_str() == nullptr) {
    return;
  }
  Global_vulkan_state::get().device().setDebugUtilsObjectNameEXT({
    .objectType = object_type,
    .objectHandle = object_handle,
    .pObjectName = name.c_str(),
  });
#endif
}

} // namespace fpsparty::graphics::detail
