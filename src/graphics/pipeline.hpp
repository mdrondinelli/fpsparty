#ifndef FPSPARTY_GRAPHICS_PIPELINE_HPP
#define FPSPARTY_GRAPHICS_PIPELINE_HPP

#include "graphics/pipeline_layout.hpp"
#include "graphics/shader.hpp"
#include "graphics/shader_stage.hpp"
#include "rc.hpp"
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
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

struct Pipeline_vertex_input_state_create_info {
  std::span<const Vertex_binding_description> bindings;
  std::span<const Vertex_attribute_description> attributes;
};

struct Pipeline_depth_state_create_info {
  bool depth_attachment_enabled;
};

enum class Color_attachment_format {
  b8g8r8a8_srgb = static_cast<int>(vk::Format::eB8G8R8A8Srgb),
};

struct Pipeline_color_state_create_info {
  std::span<const Color_attachment_format> color_attachment_formats;
};

struct Pipeline_create_info {
  std::span<Pipeline_shader_stage_create_info> shader_stages;
  Pipeline_vertex_input_state_create_info vertex_input_state;
  Pipeline_depth_state_create_info depth_state;
  Pipeline_color_state_create_info color_state;
  rc::Strong<Pipeline_layout> layout;
};

class Pipeline {
public:
  explicit Pipeline(const Pipeline_create_info &info);

  const rc::Strong<Pipeline_layout> &get_layout() const noexcept;

  vk::Pipeline get_vk_pipeline() const noexcept;

private:
  rc::Strong<Pipeline_layout> _layout;
  vk::UniquePipeline _vk_pipeline;
};
} // namespace fpsparty::graphics

#endif
