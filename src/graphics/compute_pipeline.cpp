#include "compute_pipeline.hpp"

#include "global_vulkan_state.hpp"

namespace fpsparty::graphics {

Compute_pipeline::Compute_pipeline(Compute_pipeline_create_info const &info)
    : _push_constant_range_size{info.shader->get_push_constant_range_size()} {
  auto const shader_stage = vk::PipelineShaderStageCreateInfo{
    .stage = vk::ShaderStageFlagBits::eCompute,
    .module = detail::get_shader_vk_shader_module(*info.shader),
    .pName = "main",
  };
  auto const flags_info = vk::PipelineCreateFlags2CreateInfo{
    .flags = vk::PipelineCreateFlagBits2::eDescriptorHeapEXT,
  };
  _vk_pipeline = std::move(
    Global_vulkan_state::get()
      .device()
      .createComputePipelinesUnique(
        {},
        {vk::ComputePipelineCreateInfo{
          .pNext = &flags_info,
          .stage = shader_stage,
          .layout = {},
        }})
      .value[0]);
}

} // namespace fpsparty::graphics
