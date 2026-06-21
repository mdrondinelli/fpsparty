#ifndef FPSPARTY_CLIENT_BLOCK_TEXTURE_REGISTRY_HPP
#define FPSPARTY_CLIENT_BLOCK_TEXTURE_REGISTRY_HPP

#include <vector>

#include <int.hpp>
#include <graphics/graphics.hpp>

namespace fpsparty::client {

struct Block_texture_registry_create_info {
  graphics::Graphics *graphics;
};

class Block_texture_registry {
public:
  explicit Block_texture_registry(Block_texture_registry_create_info const &info);

  // Returns the texture index. Idempotent.
  u32 add(rc::Strong<graphics::Image> image);

  void upload_descriptors(graphics::Work_recorder &recorder);

  rc::Strong<graphics::Buffer> get_descriptor_index_buffer() const noexcept;

private:
  u32 get_descriptor_index_buffer_capacity() const noexcept;

  void create_descriptor_index_buffer(u32 capacity);

  graphics::Graphics *_graphics;
  rc::Strong<graphics::Buffer> _descriptor_index_buffer;
  std::vector<rc::Strong<graphics::Image>> _images;
};

}

#endif
