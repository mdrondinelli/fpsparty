#ifndef FPSPARTY_GRAPHICS_COMPUTE_PIPELINE_HPP
#define FPSPARTY_GRAPHICS_COMPUTE_PIPELINE_HPP

#include <int.hpp>

#include <vulkan/vulkan.hpp>

#include "shader.hpp"

namespace fpsparty::graphics {
class Compute_pipeline;

namespace detail {
vk::Pipeline
get_compute_pipeline_vk_pipeline(Compute_pipeline const &pipeline) noexcept;
}

struct Compute_pipeline_create_info {
  Shader *shader;
};

class Compute_pipeline {
public:
  explicit Compute_pipeline(Compute_pipeline_create_info const &info);

  u64 get_push_constant_range_size() const noexcept {
    return _push_constant_range_size;
  }

private:
  friend vk::Pipeline
  detail::get_compute_pipeline_vk_pipeline(
    Compute_pipeline const &pipeline) noexcept;

  vk::UniquePipeline _vk_pipeline;
  u64 _push_constant_range_size{};
};

namespace detail {
inline vk::Pipeline
get_compute_pipeline_vk_pipeline(Compute_pipeline const &pipeline) noexcept {
  return *pipeline._vk_pipeline;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
