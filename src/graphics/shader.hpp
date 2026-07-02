#ifndef FPSPARTY_GRAPHICS_SHADER_HPP
#define FPSPARTY_GRAPHICS_SHADER_HPP

#include "int.hpp"

#include <cstdint>
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {

class Shader;

namespace detail {

vk::ShaderModule get_shader_vk_shader_module(Shader const &shader) noexcept;

} // namespace detail

struct Shader_create_info {
  std::span<std::uint32_t const> code;
};

class Shader {
public:
  explicit Shader(Shader_create_info const &info);

  u64 get_push_constant_range_size() const noexcept {
    return _push_constant_block_size;
  }

private:
  friend vk::ShaderModule
  detail::get_shader_vk_shader_module(Shader const &shader) noexcept;

  vk::UniqueShaderModule _vk_shader_module{};
  u64 _push_constant_block_size{};
};

Shader load_shader(char const *path);

namespace detail {
inline vk::ShaderModule
get_shader_vk_shader_module(Shader const &shader) noexcept {
  return *shader._vk_shader_module;
}

} // namespace detail

} // namespace fpsparty::graphics

#endif
