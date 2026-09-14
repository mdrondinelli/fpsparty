#ifndef FPSPARTY_GRAPHICS_COMPUTE_PIPELINE_HPP
#define FPSPARTY_GRAPHICS_COMPUTE_PIPELINE_HPP

#include <int.hpp>

#include <vulkan/vulkan.hpp>

#include "debug_name.hpp"
#include "no_unique_address.hpp"
#include "shader.hpp"

namespace fpsparty::graphics {
class Compute_pipeline;

namespace detail {
vk::Pipeline
get_compute_pipeline_vk_pipeline(Compute_pipeline const &pipeline) noexcept;
}

struct Compute_pipeline_create_info {
  Shader *shader;
  vk::PipelineLayout layout{};
  FPSPARTY_NO_UNIQUE_ADDRESS Debug_name debug_name{};
};

class Compute_pipeline {
public:
  explicit Compute_pipeline(Compute_pipeline_create_info const &info);

private:
  friend vk::Pipeline detail::get_compute_pipeline_vk_pipeline(
    Compute_pipeline const &pipeline) noexcept;

  vk::UniquePipeline _vk_pipeline;
};

namespace detail {
inline vk::Pipeline
get_compute_pipeline_vk_pipeline(Compute_pipeline const &pipeline) noexcept {
  return *pipeline._vk_pipeline;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
