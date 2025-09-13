#ifndef FPSPARTY_GRAPHICS_SYNCHRONIZATION_SCOPE_HPP
#define FPSPARTY_GRAPHICS_SYNCHRONIZATION_SCOPE_HPP

#include "flags.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Pipeline_stage_flag_bits {
  color_attachment_output = static_cast<std::uint64_t>(
    vk::PipelineStageFlagBits2::eColorAttachmentOutput
  ),
};

enum class Access_flag_bits {
  color_attachment_write =
    static_cast<std::uint64_t>(vk::AccessFlagBits2::eColorAttachmentWrite),
};

using Pipeline_stage_flags = Flags<Pipeline_stage_flag_bits>;
using Access_flags = Flags<Access_flag_bits>;

constexpr Pipeline_stage_flags
operator|(Pipeline_stage_flag_bits lhs, Pipeline_stage_flag_bits rhs) noexcept {
  return Pipeline_stage_flags{lhs} | rhs;
}

constexpr Access_flags
operator|(Access_flag_bits lhs, Access_flag_bits rhs) noexcept {
  return Access_flags{lhs} | rhs;
}

struct Synchronization_scope {
  Pipeline_stage_flags stage_mask;
  Access_flags access_mask;
};
} // namespace fpsparty::graphics

#endif
