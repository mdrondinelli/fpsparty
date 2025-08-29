#ifndef FPSPARTY_GRAPHICS_COMPARE_OP_HPP
#define FPSPARTY_GRAPHICS_COMPARE_OP_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Compare_op {
  never = static_cast<int>(vk::CompareOp::eNever),
  less = static_cast<int>(vk::CompareOp::eLess),
  equal = static_cast<int>(vk::CompareOp::eEqual),
  less_or_equal = static_cast<int>(vk::CompareOp::eLessOrEqual),
  greater = static_cast<int>(vk::CompareOp::eGreater),
  not_equal = static_cast<int>(vk::CompareOp::eNotEqual),
  greater_or_equal = static_cast<int>(vk::CompareOp::eGreaterOrEqual),
  always = static_cast<int>(vk::CompareOp::eAlways),
};
}

#endif
