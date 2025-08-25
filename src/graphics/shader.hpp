#ifndef FPSPARTY_GRAPHICS_SHADER_HPP
#define FPSPARTY_GRAPHICS_SHADER_HPP

#include <cstdint>
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Shader;

namespace detail {
vk::ShaderModule get_shader_vk_shader_module(const Shader &shader) noexcept;
}

struct Shader_create_info {
  std::span<const std::uint32_t> code;
};

class Shader {
public:
  explicit Shader(const Shader_create_info &info);

private:
  friend vk::ShaderModule
  detail::get_shader_vk_shader_module(const Shader &shader) noexcept;
  vk::UniqueShaderModule _vk_shader_module{};
};

Shader load_shader(const char *path);

namespace detail {
inline vk::ShaderModule
get_shader_vk_shader_module(const Shader &shader) noexcept {
  return *shader._vk_shader_module;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
