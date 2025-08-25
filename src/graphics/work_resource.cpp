#include "work_resource.hpp"
#include "graphics/global_vulkan_state.hpp"

namespace fpsparty::graphics::detail {
Work_resource create_work_resource() {
  const auto device = Global_vulkan_state::get().device();
  auto command_pool = device.createCommandPoolUnique({
      .flags = vk::CommandPoolCreateFlagBits::eTransient,
      .queueFamilyIndex = Global_vulkan_state::get().queue_family_index(),
  });
  const auto command_buffer = device.allocateCommandBuffers({
      .commandPool = *command_pool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  })[0];
  return {
      .vk_fence = device.createFenceUnique({}),
      .vk_command_pool = std::move(command_pool),
      .vk_command_buffer = command_buffer,
  };
}

void reset_work_resource(Work_resource &resource) {
  const auto device = Global_vulkan_state::get().device();
  device.resetFences({*resource.vk_fence});
  device.resetCommandPool(*resource.vk_command_pool);
  resource.buffers.clear();
  resource.images.clear();
  resource.pipelines.clear();
  resource.pipeline_layouts.clear();
}
} // namespace fpsparty::graphics::detail
