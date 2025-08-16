#include "command_list.hpp"
#include "graphics/buffer.hpp"
#include "rc.hpp"
#include <utility>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
Command_list::Command_list(vk::UniqueCommandBuffer command_buffer) noexcept
    : _commands{std::move(command_buffer)} {}

Command_list_release_result Command_list::release() noexcept {
  return {
      .commands = std::move(_commands),
      .buffers = std::move(_buffers),
      .pipeline_layouts = std::move(_pipeline_layouts),
  };
}

void Command_list::add_reference(rc::Strong<const Buffer> buffer) {
  _buffers.emplace_back(std::move(buffer));
}

void Command_list::add_reference(
    rc::Strong<const Pipeline_layout> pipeline_layout) {
  _pipeline_layouts.emplace_back(std::move(pipeline_layout));
}

vk::CommandBuffer Command_list::get_command_buffer() const noexcept {
  return *_commands;
}
} // namespace fpsparty::graphics
