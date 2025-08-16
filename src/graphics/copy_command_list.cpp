#include "copy_command_list.hpp"
#include "graphics/command_list.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
Copy_command_list::Copy_command_list(
    vk::UniqueCommandBuffer command_buffer) noexcept
    : Command_list{std::move(command_buffer)} {}

} // namespace fpsparty::graphics
