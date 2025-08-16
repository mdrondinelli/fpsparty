#ifndef FPSPARTY_GRAPHICS_PIPELINE_LAYOUT_HPP
#define FPSPARTY_GRAPHICS_PIPELINE_LAYOUT_HPP

#include "graphics/shader_stage.hpp"
#include <cstdint>
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
struct Push_constant_range {
  Shader_stage_flags stage_flags;
  std::uint32_t offset;
  std::uint32_t size;
};

struct Pipeline_layout_create_info {
  std::span<const Push_constant_range> push_constant_ranges;
};

class Pipeline_layout {
public:
  explicit Pipeline_layout(const Pipeline_layout_create_info &info);

  vk::PipelineLayout get_vk_pipeline_layout() const noexcept;

private:
  vk::UniquePipelineLayout _vk_pipeline_layout{};
};
} // namespace fpsparty::graphics

#endif
