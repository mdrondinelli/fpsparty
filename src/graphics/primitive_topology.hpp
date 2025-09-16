#ifndef FPSPARTY_GRAPHICS_PRIMITIVE_TOPOLOGY_HPP
#define FPSPARTY_GRAPHICS_PRIMITIVE_TOPOLOGY_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Primitive_topology {
  line_list = static_cast<int>(vk::PrimitiveTopology::eLineList),
  triangle_list = static_cast<int>(vk::PrimitiveTopology::eTriangleList),
};
}

#endif
