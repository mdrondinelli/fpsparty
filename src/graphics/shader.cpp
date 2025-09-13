#include "shader.hpp"
#include "graphics/global_vulkan_state.hpp"
#include <fstream>

namespace fpsparty::graphics {
Shader::Shader(const Shader_create_info &info)
    : _vk_shader_module{
        Global_vulkan_state::get().device().createShaderModuleUnique({
          .codeSize = static_cast<std::uint32_t>(info.code.size_bytes()),
          .pCode = info.code.data(),
        })
      } {}

Shader load_shader(const char *path) {
  auto input_stream = std::ifstream{};
  input_stream.exceptions(std::ios::badbit | std::ios::failbit);
  input_stream.open(path, std::ios::binary | std::ios::ate);
  const auto size = input_stream.tellg();
  auto bytecode = std::vector<std::uint32_t>{};
  bytecode.resize(size / sizeof(std::uint32_t));
  input_stream.seekg(0);
  input_stream.read(reinterpret_cast<char *>(bytecode.data()), size);
  return Shader{{.code = bytecode}};
}
} // namespace fpsparty::graphics
