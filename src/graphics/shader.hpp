#ifndef FPSPARTY_GRAPHICS_SHADER_HPP
#define FPSPARTY_GRAPHICS_SHADER_HPP

#include <cstdint>
#include <span>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace fpsparty::graphics {
struct Shader_create_info {
  std::span<const std::uint32_t> code;
};

class Shader {
public:
  explicit Shader(const Shader_create_info &info);

  vk::ShaderModule get_vk_shader_module() const noexcept;

private:
  vk::UniqueShaderModule _vk_shader_module{};
};

Shader load_shader(const char *path);
} // namespace fpsparty::graphics

#endif
