#include "work_resource.hpp"
#include "graphics/global_vulkan_state.hpp"

namespace fpsparty::graphics::detail {
void reset_work_resource(Work_resource &resource) {
  Global_vulkan_state::get().device().resetFences({*resource.vk_fence});
  Global_vulkan_state::get().device().resetCommandPool(
      *resource.vk_command_pool);
  resource.buffers.clear();
  resource.images.clear();
  resource.pipelines.clear();
  resource.pipeline_layouts.clear();
}
} // namespace fpsparty::graphics::detail
