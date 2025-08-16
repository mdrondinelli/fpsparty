#include "pipeline_layout.hpp"
#include "graphics/global_vulkan_state.hpp"
#include <vector>
#include <vulkan/vulkan_structs.hpp>

namespace fpsparty::graphics {
Pipeline_layout::Pipeline_layout(const Pipeline_layout_create_info &info) {
  auto vk_push_constant_ranges = std::vector<vk::PushConstantRange>{};
  vk_push_constant_ranges.reserve(info.push_constant_ranges.size());
  for (const auto &push_constant_range : info.push_constant_ranges) {
    vk_push_constant_ranges.emplace_back(vk::PushConstantRange{
        .stageFlags =
            static_cast<vk::ShaderStageFlags>(push_constant_range.stage_flags),
        .offset = push_constant_range.offset,
        .size = push_constant_range.size,
    });
  }
  _vk_pipeline_layout =
      Global_vulkan_state::get().device().createPipelineLayoutUnique({
          .pushConstantRangeCount =
              static_cast<std::uint32_t>(vk_push_constant_ranges.size()),
          .pPushConstantRanges = vk_push_constant_ranges.data(),
      });
}

vk::PipelineLayout Pipeline_layout::get_vk_pipeline_layout() const noexcept {
  return *_vk_pipeline_layout;
}
} // namespace fpsparty::graphics
