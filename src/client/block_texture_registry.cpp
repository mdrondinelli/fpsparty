#include "block_texture_registry.hpp"

#include <cstring>

namespace fpsparty::client {

Block_texture_registry::Block_texture_registry(
  Block_texture_registry_create_info const &info)
    : _graphics{info.graphics} {
  reserve(1024);
}

u32 Block_texture_registry::add(rc::Strong<graphics::Image> image) {
  auto it = std::ranges::find(_images, image);
  if (it != _images.end()) {
    return static_cast<u32>(it - _images.begin());
  } else {
    auto const retval = static_cast<u32>(_images.size());
    auto const capacity = get_buffer_capacity();
    if (retval + 1 > capacity) {
      reserve(capacity * 2);
    }
    auto descriptor = _graphics->create_sampled_image_descriptor(image);
    auto const memory = _descriptor_index_buffer->map();
    auto const handle = descriptor->get_handle();
    std::memcpy(
      memory.get().data() + sizeof(u32) * retval,
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

void Block_texture_registry::reserve(u32 capacity) {
  _descriptor_index_buffer = _graphics->create_buffer({
    .size = sizeof(u32) * capacity,
    .usage = graphics::Buffer_usage_flag_bits::shader_device_address,
    .mapping_mode = graphics::Mapping_mode::write_only,
  });
  if (!_descriptors.empty()) {
    auto const memory = _descriptor_index_buffer->map();
    for (auto i = std::size_t{}; i != _descriptors.size(); ++i) {
      auto const handle = _descriptors[i]->get_handle();
      std::memcpy(
        memory.get().data() + sizeof(u32) * i,
        &handle,
        sizeof(handle));
    }
  }
}

} // namespace fpsparty::client
