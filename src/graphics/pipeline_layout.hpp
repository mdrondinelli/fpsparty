#ifndef FPSPARTY_GRAPHICS_PIPELINE_LAYOUT_HPP
#define FPSPARTY_GRAPHICS_PIPELINE_LAYOUT_HPP

#include "graphics/shader_stage.hpp"
#include <cstdint>
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Pipeline_layout;

namespace detail {
vk::PipelineLayout get_pipeline_layout_vk_pipeline_layout(
  Pipeline_layout const &pipeline_layout) noexcept;
}

struct Push_constant_range {
  Shader_stage_flags stage_flags;
  std::uint32_t offset;
  std::uint32_t size;
};

struct Pipeline_layout_create_info {
  std::span<Push_constant_range const> push_constant_ranges;
};

class Pipeline_layout {
public:
  explicit Pipeline_layout(Pipeline_layout_create_info const &info);

private:
  friend vk::PipelineLayout detail::get_pipeline_layout_vk_pipeline_layout(
    Pipeline_layout const &pipeline_layout) noexcept;

  vk::UniquePipelineLayout _vk_pipeline_layout{};
};

namespace detail {
inline vk::PipelineLayout get_pipeline_layout_vk_pipeline_layout(
  Pipeline_layout const &pipeline_layout) noexcept {
  return *pipeline_layout._vk_pipeline_layout;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
