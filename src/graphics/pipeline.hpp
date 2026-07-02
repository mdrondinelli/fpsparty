#ifndef FPSPARTY_GRAPHICS_PIPELINE_HPP
#define FPSPARTY_GRAPHICS_PIPELINE_HPP

#include "graphics/image_format.hpp"
#include "graphics/primitive_topology.hpp"
#include "graphics/shader.hpp"
#include "graphics/shader_stage.hpp"
#include "int.hpp"
#include "rc.hpp"
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Pipeline;

namespace detail {
vk::Pipeline get_pipeline_vk_pipeline(Pipeline const &pipeline) noexcept;
}

struct Pipeline_shader_stage_create_info {
  Shader_stage_flag_bits stage;
  Shader *shader;
};

struct Pipeline_input_assembly_state_create_info {
  Primitive_topology primitive_topology;
};

struct Pipeline_depth_state_create_info {
  bool depth_attachment_enabled;
};

struct Pipeline_color_state_create_info {
  std::span<Image_format const> color_attachment_formats;
};

struct Pipeline_create_info {
  std::span<Pipeline_shader_stage_create_info const> shader_stages;
  Pipeline_input_assembly_state_create_info input_assembly_state;
  Pipeline_depth_state_create_info depth_state;
  Pipeline_color_state_create_info color_state;
};

class Pipeline {
public:
  explicit Pipeline(Pipeline_create_info const &info);

  u64 get_push_constant_range_size() const noexcept {
    return _push_constant_range_size;
  }

private:
  friend vk::Pipeline
  detail::get_pipeline_vk_pipeline(Pipeline const &pipeline) noexcept;

  vk::UniquePipeline _vk_pipeline;
  u64 _push_constant_range_size{};
};

namespace detail {
inline vk::Pipeline
get_pipeline_vk_pipeline(Pipeline const &pipeline) noexcept {
  return *pipeline._vk_pipeline;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
