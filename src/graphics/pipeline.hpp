#ifndef FPSPARTY_GRAPHICS_PIPELINE_HPP
#define FPSPARTY_GRAPHICS_PIPELINE_HPP

#include "graphics/image_format.hpp"
#include "graphics/pipeline_layout.hpp"
#include "graphics/primitive_topology.hpp"
#include "graphics/shader.hpp"
#include "graphics/shader_stage.hpp"
#include "rc.hpp"
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Pipeline;

namespace detail {
vk::Pipeline get_pipeline_vk_pipeline(const Pipeline &pipeline) noexcept;
}

struct Pipeline_shader_stage_create_info {
  Shader_stage_flag_bits stage;
  Shader *shader;
};

struct Vertex_binding_description {
  std::uint32_t binding;
  std::uint32_t stride;
};

enum class Vertex_attribute_format {
  r32g32b32_sfloat = static_cast<int>(vk::Format::eR32G32B32Sfloat)
};

struct Vertex_attribute_description {
  std::uint32_t location;
  std::uint32_t binding;
  Vertex_attribute_format format;
  std::uint32_t offset;
};

struct Graphics_pipeline_vertex_input_state_create_info {
  std::span<const Vertex_binding_description> bindings;
  std::span<const Vertex_attribute_description> attributes;
};

struct Graphics_pipeline_input_assembly_state_create_info {
  Primitive_topology primitive_topology;
};

struct Graphics_pipeline_depth_state_create_info {
  bool depth_attachment_enabled;
};

struct Graphics_pipeline_color_state_create_info {
  std::span<const Image_format> color_attachment_formats;
};

struct Graphics_pipeline_create_info {
  std::span<const Pipeline_shader_stage_create_info> shader_stages;
  Graphics_pipeline_vertex_input_state_create_info vertex_input_state;
  Graphics_pipeline_input_assembly_state_create_info input_assembly_state;
  Graphics_pipeline_depth_state_create_info depth_state;
  Graphics_pipeline_color_state_create_info color_state;
  rc::Strong<Pipeline_layout> layout;
};

struct Compute_pipeline_create_info {
  Pipeline_shader_stage_create_info shader_stage;
  rc::Strong<Pipeline_layout> layout;
};

class Pipeline {
public:
  explicit Pipeline(const Graphics_pipeline_create_info &info);

  explicit Pipeline(const Compute_pipeline_create_info &info);

  const rc::Strong<Pipeline_layout> &get_layout() const noexcept;

private:
  friend vk::Pipeline
  detail::get_pipeline_vk_pipeline(const Pipeline &pipeline) noexcept;

  rc::Strong<Pipeline_layout> _layout;
  vk::UniquePipeline _vk_pipeline;
};

namespace detail {
inline vk::Pipeline
get_pipeline_vk_pipeline(const Pipeline &pipeline) noexcept {
  return *pipeline._vk_pipeline;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
