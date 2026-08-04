#include "block_texture_registry.hpp"

#include <algorithm>
#include <cassert>
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
    auto const memory = _descriptor_index_buffer->map();
    auto const handle = image->get_sampled_descriptor_index();
    std::memcpy(
      memory.get().data() + sizeof(u32) * retval, &handle, sizeof(handle));
    _images.push_back(std::move(image));
    return retval;
  }
}

void Block_texture_registry::add_references(graphics::Work_recorder &recorder) {
  for (auto const &image : _images) {
    recorder.add_reference(image);
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
  if (!_images.empty()) {
    auto const memory = _descriptor_index_buffer->map();
    for (auto i = std::size_t{}; i != _images.size(); ++i) {
      auto const handle = _images[i]->get_sampled_descriptor_index();
      std::memcpy(
        memory.get().data() + sizeof(u32) * i, &handle, sizeof(handle));
    }
  }
}

} // namespace fpsparty::client
