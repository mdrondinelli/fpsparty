#include "block_texture_registry.hpp"

#include <cstring>

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
    auto descriptor = _graphics->create_sampled_image_descriptor(image);
    auto const descriptor_index_buffer_memory = _descriptor_index_buffer->map();
    auto const handle = descriptor->get_handle();
    std::memcpy(
      descriptor_index_buffer_memory.get().data() + sizeof(u32) * retval,
      &handle,
      sizeof(handle));
    _images.push_back(std::move(image));
    _descriptors.push_back(std::move(descriptor));
    return retval;
  }
}

void Block_texture_registry::add_references(graphics::Work_recorder &recorder) {
  for (auto const &descriptor : _descriptors) {
    recorder.add_reference(descriptor);
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
  auto old_buffer = std::move(_descriptor_index_buffer);
  _descriptor_index_buffer = _graphics->create_buffer({
    .size = sizeof(u32) * capacity,
    .usage = graphics::Buffer_usage_flag_bits::shader_device_address,
    .mapping_mode = graphics::Mapping_mode::write_only,
  });
  if (old_buffer) {
    auto const src = old_buffer->map();
    auto const dst = _descriptor_index_buffer->map();
    std::memcpy(dst.get().data(), src.get().data(), old_buffer->get_size());
  }
}

} // namespace fpsparty::client
