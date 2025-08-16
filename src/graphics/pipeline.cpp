#include "pipeline.hpp"
#include "global_vulkan_state.hpp"
#include <vector>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
Pipeline::Pipeline(const Pipeline_create_info &info) : _layout{info.layout} {
  auto vk_shader_stages = std::vector<vk::PipelineShaderStageCreateInfo>{};
  vk_shader_stages.reserve(info.shader_stages.size());
  for (const auto &shader_stage : info.shader_stages) {
    vk_shader_stages.push_back({
        .stage = static_cast<vk::ShaderStageFlagBits>(shader_stage.stage),
        .module = shader_stage.shader->get_vk_shader_module(),
        .pName = "main",
    });
  }
  auto vk_vertex_bindings = std::vector<vk::VertexInputBindingDescription>{};
  vk_vertex_bindings.reserve(info.vertex_input_state.bindings.size());
  for (const auto &vertex_binding : info.vertex_input_state.bindings) {
    vk_vertex_bindings.push_back({
        .binding = vertex_binding.binding,
        .stride = vertex_binding.stride,
        .inputRate = vk::VertexInputRate::eVertex,
    });
  }
  auto vk_vertex_attributes =
      std::vector<vk::VertexInputAttributeDescription>{};
  vk_vertex_attributes.reserve(info.vertex_input_state.attributes.size());
  for (const auto &vertex_attribute : info.vertex_input_state.attributes) {
    vk_vertex_attributes.push_back({
        .location = vertex_attribute.location,
        .binding = vertex_attribute.binding,
        .format = static_cast<vk::Format>(vertex_attribute.format),
        .offset = vertex_attribute.offset,
    });
  }
  const auto vk_vertex_input_state = vk::PipelineVertexInputStateCreateInfo{
      .vertexBindingDescriptionCount =
          static_cast<std::uint32_t>(vk_vertex_bindings.size()),
      .pVertexBindingDescriptions = vk_vertex_bindings.data(),
      .vertexAttributeDescriptionCount =
          static_cast<std::uint32_t>(vk_vertex_attributes.size()),
      .pVertexAttributeDescriptions = vk_vertex_attributes.data(),
  };
  const auto vk_input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo{
      .topology = vk::PrimitiveTopology::eTriangleList,
      .primitiveRestartEnable = vk::False,
  };
  const auto vk_viewport_state = vk::PipelineViewportStateCreateInfo{
      .viewportCount = 1,
      .scissorCount = 1,
  };
  const auto vk_rasterization_state = vk::PipelineRasterizationStateCreateInfo{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .depthBiasEnable = vk::False,
      .lineWidth = 1.0f,
  };
  const auto vk_multisample_state = vk::PipelineMultisampleStateCreateInfo{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
  };
  const auto vk_depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{};
  auto vk_color_blend_attachment_states =
      std::vector<vk::PipelineColorBlendAttachmentState>{};
  vk_color_blend_attachment_states.reserve(
      info.color_state.color_attachment_formats.size());
  for (auto i = std::size_t{};
       i != info.color_state.color_attachment_formats.size(); ++i) {
    vk_color_blend_attachment_states.push_back({
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    });
  }
  const auto vk_color_blend_state = vk::PipelineColorBlendStateCreateInfo{
      .attachmentCount =
          static_cast<std::uint32_t>(vk_color_blend_attachment_states.size()),
      .pAttachments = vk_color_blend_attachment_states.data(),
  };
  const auto always_dynamic_states = std::array<vk::DynamicState, 5>{
      vk::DynamicState::ePrimitiveTopology, vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,           vk::DynamicState::eCullMode,
      vk::DynamicState::eFrontFace,
  };
  const auto conditionally_dynamic_states = std::array<vk::DynamicState, 3>{
      vk::DynamicState::eDepthTestEnable,
      vk::DynamicState::eDepthWriteEnable,
      vk::DynamicState::eDepthCompareOp,
  };
  auto vk_dynamic_states = std::vector<vk::DynamicState>{};
  vk_dynamic_states.reserve(always_dynamic_states.size() +
                            (info.depth_state.depth_attachment_enabled
                                 ? conditionally_dynamic_states.size()
                                 : 0));
  vk_dynamic_states.append_range(always_dynamic_states);
  if (info.depth_state.depth_attachment_enabled) {
    vk_dynamic_states.append_range(conditionally_dynamic_states);
  }
  const auto vk_dynamic_state = vk::PipelineDynamicStateCreateInfo{
      .dynamicStateCount = static_cast<std::uint32_t>(vk_dynamic_states.size()),
      .pDynamicStates = vk_dynamic_states.data(),
  };
  auto vk_color_attachment_formats = std::vector<vk::Format>{};
  vk_color_attachment_formats.reserve(
      info.color_state.color_attachment_formats.size());
  for (const auto &color_attachment_format :
       info.color_state.color_attachment_formats) {
    vk_color_attachment_formats.emplace_back(
        static_cast<vk::Format>(color_attachment_format));
  }
  const auto vk_rendering_info = vk::PipelineRenderingCreateInfo{
      .colorAttachmentCount =
          static_cast<std::uint32_t>(vk_color_attachment_formats.size()),
      .pColorAttachmentFormats = vk_color_attachment_formats.data(),
      .depthAttachmentFormat = info.depth_state.depth_attachment_enabled
                                   ? vk::Format::eD32Sfloat
                                   : vk::Format::eUndefined,
  };
  _vk_pipeline =
      std::move(Global_vulkan_state::get()
                    .device()
                    .createGraphicsPipelinesUnique(
                        {}, {vk::GraphicsPipelineCreateInfo{
                                .pNext = &vk_rendering_info,
                                .stageCount = static_cast<std::uint32_t>(
                                    vk_shader_stages.size()),
                                .pStages = vk_shader_stages.data(),
                                .pVertexInputState = &vk_vertex_input_state,
                                .pInputAssemblyState = &vk_input_assembly_state,
                                .pViewportState = &vk_viewport_state,
                                .pRasterizationState = &vk_rasterization_state,
                                .pMultisampleState = &vk_multisample_state,
                                .pDepthStencilState = &vk_depth_stencil_state,
                                .pColorBlendState = &vk_color_blend_state,
                                .pDynamicState = &vk_dynamic_state,
                                .layout = info.layout->get_vk_pipeline_layout(),
                            }})
                    .value[0]);
}
} // namespace fpsparty::graphics
