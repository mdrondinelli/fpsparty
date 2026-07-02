#include "shader.hpp"

#include "graphics/global_vulkan_state.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

#include <spirv_reflect.h>

namespace fpsparty::graphics {

namespace {

class Reflected_shader_module {
public:
  explicit Reflected_shader_module(std::span<std::uint32_t const> code) {
    auto const result =
      spvReflectCreateShaderModule(code.size_bytes(), code.data(), &_module);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
      throw std::runtime_error{"Failed to reflect SPIR-V shader module."};
    }
  }

  Reflected_shader_module(Reflected_shader_module const &) = delete;
  Reflected_shader_module &operator=(Reflected_shader_module const &) = delete;

  ~Reflected_shader_module() { spvReflectDestroyShaderModule(&_module); }

  SpvReflectShaderModule const &get() const noexcept { return _module; }

private:
  SpvReflectShaderModule _module{};
};

void check_spv_reflect_result(SpvReflectResult result) {
  if (result != SPV_REFLECT_RESULT_SUCCESS) {
    throw std::runtime_error{"Failed to reflect shader push constants."};
  }
}

} // namespace

Shader::Shader(Shader_create_info const &info)
    : _vk_shader_module{Global_vulkan_state::get()
                          .device()
                          .createShaderModuleUnique({
                            .codeSize = static_cast<std::uint32_t>(
                              info.code.size_bytes()),
                            .pCode = info.code.data(),
                          })},
      _push_constant_block_size{[&] {
        auto const module = Reflected_shader_module{info.code};
        auto block_count = std::uint32_t{};
        check_spv_reflect_result(
          spvReflectEnumerateEntryPointPushConstantBlocks(
            &module.get(), "main", &block_count, nullptr));
        if (block_count == 0) {
          return u64{};
        }
        if (block_count > 1) {
          throw std::runtime_error{
            "Shaders may only use one push constant block."};
        }
        auto blocks = std::vector<SpvReflectBlockVariable *>{};
        blocks.resize(block_count);
        check_spv_reflect_result(
          spvReflectEnumerateEntryPointPushConstantBlocks(
            &module.get(), "main", &block_count, blocks.data()));
        auto const &block = *blocks[0];
        if (block.offset != 0) {
          throw std::runtime_error{
            "Shader push constant blocks must start at offset 0."};
        }
        return static_cast<u64>(block.size);
      }()} {}

Shader load_shader(char const *path) {
  auto input_stream = std::ifstream{};
  input_stream.exceptions(std::ios::badbit | std::ios::failbit);
  input_stream.open(path, std::ios::binary | std::ios::ate);
  auto const size = input_stream.tellg();
  auto bytecode = std::vector<std::uint32_t>{};
  bytecode.resize(size / sizeof(std::uint32_t));
  input_stream.seekg(0);
  input_stream.read(reinterpret_cast<char *>(bytecode.data()), size);
  return Shader{{.code = bytecode}};
}
} // namespace fpsparty::graphics
