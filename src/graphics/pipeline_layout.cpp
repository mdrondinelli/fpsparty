#include "pipeline_layout.hpp"
#include "graphics/descriptor_set_layout.hpp"
#include "graphics/global_vulkan_state.hpp"
#include <vector>

namespace fpsparty::graphics {
Pipeline_layout::Pipeline_layout(const Pipeline_layout_create_info &info) {
  auto vk_set_layouts = std::vector<vk::DescriptorSetLayout>{};
  vk_set_layouts.reserve(info.set_layouts.size());
  for (const auto &set_layout : info.set_layouts) {
    vk_set_layouts.emplace_back(
      detail::get_descriptor_set_layout_vk_descriptor_set_layout(*set_layout));
  }
  auto vk_push_constant_ranges = std::vector<vk::PushConstantRange>{};
  vk_push_constant_ranges.reserve(info.push_constant_ranges.size());
  for (const auto &push_constant_range : info.push_constant_ranges) {
    vk_push_constant_ranges.push_back({
      .stageFlags =
        static_cast<vk::ShaderStageFlags>(push_constant_range.stage_flags),
      .offset = push_constant_range.offset,
      .size = push_constant_range.size,
    });
  }
  _vk_pipeline_layout =
    Global_vulkan_state::get().device().createPipelineLayoutUnique({
      .setLayoutCount = static_cast<std::uint32_t>(vk_set_layouts.size()),
      .pSetLayouts = vk_set_layouts.data(),
      .pushConstantRangeCount =
        static_cast<std::uint32_t>(vk_push_constant_ranges.size()),
      .pPushConstantRanges = vk_push_constant_ranges.data(),
    });
}
} // namespace fpsparty::graphics
