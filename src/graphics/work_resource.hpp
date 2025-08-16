#ifndef FPSPARTY_GRAPHICS_WORK_RESOURCE_HPP
#define FPSPARTY_GRAPHICS_WORK_RESOURCE_HPP

#include "graphics/buffer.hpp"
#include "graphics/image.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/pipeline_layout.hpp"
#include "rc.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics::detail {
struct Work_resource {
  vk::UniqueFence vk_fence;
  vk::UniqueCommandPool vk_command_pool;
  vk::CommandBuffer vk_command_buffer;
  std::vector<rc::Strong<const Buffer>> buffers;
  std::vector<rc::Strong<const Image>> images;
  std::vector<rc::Strong<const Pipeline>> pipelines;
  std::vector<rc::Strong<const Pipeline_layout>> pipeline_layouts;
};

Work_resource create_work_resource();

void reset_work_resource(Work_resource &resource);
} // namespace fpsparty::graphics::detail

#endif
