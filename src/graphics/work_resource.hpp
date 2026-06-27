#ifndef FPSPARTY_GRAPHICS_WORK_RESOURCE_HPP
#define FPSPARTY_GRAPHICS_WORK_RESOURCE_HPP

#include <optional>

#include <vulkan/vulkan.hpp>

#include <rc.hpp>

#include "buffer.hpp"
#include "descriptor_heap.hpp"
#include "image.hpp"
#include "pipeline.hpp"
#include "work_done_callback.hpp"

namespace fpsparty::graphics::detail {

struct Work_resource {
  vk::UniqueFence vk_fence;
  vk::UniqueCommandPool vk_command_pool;
  vk::CommandBuffer vk_command_buffer;
  std::optional<Descriptor_allocation> descriptor_allocation{};
  std::vector<rc::Strong<Buffer const>> buffers{};
  std::vector<rc::Strong<Image const>> images{};
  std::vector<rc::Strong<Pipeline const>> pipelines{};
  std::vector<Work_done_callback *> done_callbacks{};
};

Work_resource create_work_resource();

void reset_work_resource(Work_resource &resource);

} // namespace fpsparty::graphics::detail

#endif
