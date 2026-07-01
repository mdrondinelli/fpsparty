#include "block_texture_registry.hpp"

namespace fpsparty::client {

Block_texture_registry::Block_texture_registry(
  Block_texture_registry_create_info const &info)
    : _graphics{info.graphics} {
  create_buffer(1024);
}

u32 Block_texture_registry::add(rc::Strong<graphics::Image> image) {
  auto it = std::ranges::find(_images, image);
  if (it != _images.end()) {
    return static_cast<u32>(it - _images.begin());
  } else {
    auto const retval = static_cast<u32>(_images.size());
    auto const capacity = get_buffer_capacity();
    if (retval + 1 > capacity) {
      create_buffer(capacity * 2);
    }
    _images.push_back(std::move(image));
    return retval;
  }
}

void Block_texture_registry::upload_descriptors(
  graphics::Work_recorder &recorder) {
  assert(_descriptor_index_buffer);
  assert(_descriptor_index_buffer->get_size() >= sizeof(u32) * _images.size());
  auto const index_buffer_memory = _descriptor_index_buffer->map();
  auto dst = index_buffer_memory.get().data();
  for (auto const &image : _images) {
    auto const index = recorder.upload_sampled_image_descriptor(image);
    std::memcpy(dst, &index, sizeof(u32));
    dst += sizeof(u32);
  }
}

rc::Strong<graphics::Buffer>
Block_texture_registry::get_buffer() const noexcept {
  return _descriptor_index_buffer;
}

u32 Block_texture_registry::get_buffer_capacity() const noexcept {
  assert(_descriptor_index_buffer);
  return _descriptor_index_buffer->get_size() / sizeof(u32);
}

void Block_texture_registry::create_buffer(u32 capacity) {
  _descriptor_index_buffer = _graphics->create_buffer({
    .size = sizeof(u32) * capacity,
    .usage = graphics::Buffer_usage_flag_bits::shader_device_address,
    .mapping_mode = graphics::Mapping_mode::write_only,
  });
}

} // namespace fpsparty::client
