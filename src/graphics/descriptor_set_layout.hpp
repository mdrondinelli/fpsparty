#ifndef FPSPARTY_GRAPHICS_DESCRIPTOR_SET_LAYOUT_HPP
#define FPSPARTY_GRAPHICS_DESCRIPTOR_SET_LAYOUT_HPP

#include "graphics/descriptor_type.hpp"
#include "graphics/shader_stage.hpp"
#include <cstdint>
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Descriptor_set_layout;

namespace detail {
vk::DescriptorSetLayout get_descriptor_set_layout_vk_descriptor_set_layout(
  const Descriptor_set_layout &layout);
}

struct Descriptor_set_layout_binding {
  std::uint32_t binding;
  Descriptor_type descriptor_type;
  std::uint32_t descriptor_count;
  Shader_stage_flags stage_flags;
};

struct Descriptor_set_layout_create_info {
  std::span<const Descriptor_set_layout_binding> bindings;
};

class Descriptor_set_layout {
public:
  explicit Descriptor_set_layout(const Descriptor_set_layout_create_info &info);

private:
  friend vk::DescriptorSetLayout
  detail::get_descriptor_set_layout_vk_descriptor_set_layout(
    const Descriptor_set_layout &layout);

  vk::UniqueDescriptorSetLayout _vk_descriptor_set_layout{};
};

namespace detail {
inline vk::DescriptorSetLayout
get_descriptor_set_layout_vk_descriptor_set_layout(
  const Descriptor_set_layout &layout) {
  return *layout._vk_descriptor_set_layout;
}
} // namespace detail
} // namespace fpsparty::graphics

#endif
