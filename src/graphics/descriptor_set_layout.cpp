#include "descriptor_set_layout.hpp"
#include "graphics/global_vulkan_state.hpp"
#include <vector>

namespace fpsparty::graphics {
Descriptor_set_layout::Descriptor_set_layout(
  const Descriptor_set_layout_create_info &info) {
  auto vk_bindings = std::vector<vk::DescriptorSetLayoutBinding>{};
  vk_bindings.reserve(info.bindings.size());
  for (const auto &binding : info.bindings) {
    vk_bindings.push_back({
      .binding = binding.binding,
      .descriptorType =
        static_cast<vk::DescriptorType>(binding.descriptor_type),
      .descriptorCount = binding.descriptor_count,
      .stageFlags = static_cast<vk::ShaderStageFlags>(binding.stage_flags),
    });
  }
  _vk_descriptor_set_layout =
    Global_vulkan_state::get().device().createDescriptorSetLayoutUnique({
      .bindingCount = static_cast<std::uint32_t>(vk_bindings.size()),
      .pBindings = vk_bindings.data(),
    });
}
} // namespace fpsparty::graphics
