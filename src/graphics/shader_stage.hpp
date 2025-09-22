#ifndef FPSPARTY_GRAPHICS_SHADER_STAGE_HPP
#define FPSPARTY_GRAPHICS_SHADER_STAGE_HPP

#include "flags.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Shader_stage_flag_bits {
  vertex = static_cast<int>(vk::ShaderStageFlagBits::eVertex),
  fragment = static_cast<int>(vk::ShaderStageFlagBits::eFragment),
  compute = static_cast<int>(vk::ShaderStageFlagBits::eCompute),
};

using Shader_stage_flags = Flags<Shader_stage_flag_bits>;

constexpr Shader_stage_flags
operator|(Shader_stage_flag_bits lhs, Shader_stage_flag_bits rhs) noexcept {
  return Shader_stage_flags{
    static_cast<Shader_stage_flags::Value>(lhs) |
    static_cast<Shader_stage_flags::Value>(rhs)};
}
} // namespace fpsparty::graphics

#endif
