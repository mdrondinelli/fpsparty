#include "texture_manager.hpp"

#include <fstream>

#include <ppm/ppm.hpp>

namespace fpsparty::client {

namespace {

std::vector<std::byte> load_file(char const *path) {
  auto file = std::ifstream{path, std::ios::binary | std::ios::ate};
  if (!file) {
    throw std::runtime_error{std::string{"Failed to open file: "} + path};
  }
  auto const size = file.tellg();
  if (size < 0) {
    throw std::runtime_error{std::string{"Failed to size file: "} + path};
  }
  auto data = std::vector<std::byte>(static_cast<std::size_t>(size));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
    throw std::runtime_error{std::string{"Failed to read file: "} + path};
  }
  return data;
}

rc::Strong<graphics::Image> load_texture(graphics::Graphics &graphics, char const *path) {
  auto const file = load_file(path);
  auto const ppm_image = ppm::load_ppm(file);
  auto pixels = std::vector<std::byte>(
    static_cast<std::size_t>(ppm_image.width) *
    static_cast<std::size_t>(ppm_image.height) * 4);
  for (auto i = std::size_t{};
       i != static_cast<std::size_t>(ppm_image.width) *
              static_cast<std::size_t>(ppm_image.height);
       ++i) {
    pixels[i * 4 + 0] = ppm_image.data[i * 3 + 2];
    pixels[i * 4 + 1] = ppm_image.data[i * 3 + 1];
    pixels[i * 4 + 2] = ppm_image.data[i * 3 + 0];
    pixels[i * 4 + 3] = static_cast<std::byte>(0xff);
  }
  auto image = graphics.create_image({
    .dimensionality = 2,
    .format = graphics::Image_format::b8g8r8a8_srgb,
    .extent = {ppm_image.width, ppm_image.height, 1},
    .mip_level_count = 1,
    .array_layer_count = 1,
    .usage = graphics::Image_usage_flag_bits::sampled |
             graphics::Image_usage_flag_bits::transfer_dst,
  });
  auto const staging_buffer = graphics.create_staging_buffer(pixels);
  auto work_recorder = graphics.record_transient_work();
  work_recorder.transition_image_layout(
    {},
    {
      .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
      .access_mask = graphics::Access_flag_bits::transfer_write,
    },
    graphics::Image_layout::undefined,
    graphics::Image_layout::general,
    image);
  work_recorder.copy_buffer_to_image(
    staging_buffer,
    image,
    {
      .src_offset = 0,
      .dst_mip_level = 0,
      .dst_base_array_layer = 0,
      .dst_array_layer_count = 1,
      .dst_offset = {0, 0, 0},
      .dst_extent = {ppm_image.width, ppm_image.height, 1},
    });
  work_recorder.barrier(
    {
      .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
      .access_mask = graphics::Access_flag_bits::transfer_write,
    },
    {
      .stage_mask = graphics::Pipeline_stage_flag_bits::fragment_shader,
      .access_mask = graphics::Access_flag_bits::shader_sampled_read,
    });
  auto work = graphics.submit_transient_work(std::move(work_recorder));
  work->await();
  return image;
}

}

Texture_manager::Texture_manager(Texture_manager_create_info const &info) {
  auto const texture_filenames = std::array<char const *, 5>{
    "./assets/textures/placeholder.ppm",
    "./assets/textures/stone.ppm",
    "./assets/textures/dirt.ppm",
    "./assets/textures/conveyor.ppm",
    "./assets/textures/conveyor_side.ppm",
  };
  for (auto const &texture_filename : texture_filenames) {
    _images.emplace_back(load_texture(*info.graphics, texture_filename));
  }
}

rc::Strong<graphics::Image> Texture_manager::get(Texture texture) const noexcept {
  return _images[static_cast<std::size_t>(texture)];
}

}
