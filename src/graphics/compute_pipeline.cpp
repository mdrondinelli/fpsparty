#include "compute_pipeline.hpp"

#include "global_vulkan_state.hpp"

namespace fpsparty::graphics {

Compute_pipeline::Compute_pipeline(Compute_pipeline_create_info const &info) {
  auto const shader_stage = vk::PipelineShaderStageCreateInfo{
    .stage = vk::ShaderStageFlagBits::eCompute,
    .module = detail::get_shader_vk_shader_module(*info.shader),
    .pName = "main",
  };
  _vk_pipeline = std::move(
    Global_vulkan_state::get()
      .device()
      .createComputePipelinesUnique(
        {},
        {vk::ComputePipelineCreateInfo{
          .stage = shader_stage,
          .layout = info.layout,
        }})
      .value[0]);
}

} // namespace fpsparty::graphics
