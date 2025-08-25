#ifndef FPSPARTY_GRAPHICS_FRONT_FACE_HPP
#define FPSPARTY_GRAPHICS_FRONT_FACE_HPP

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
enum class Front_face {
  counter_clockwise = static_cast<int>(vk::FrontFace::eCounterClockwise),
  clockwise = static_cast<int>(vk::FrontFace::eClockwise),
};
}

#endif
