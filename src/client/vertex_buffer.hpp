#ifndef FPSPARTY_CLIENT_VERTEX_BUFFER_HPP
#define FPSPARTY_CLIENT_VERTEX_BUFFER_HPP

#include "client/graphics_buffer.hpp"
#include "rc.hpp"

namespace fpsparty::client {
class Vertex_buffer : public rc::Object<Vertex_buffer>, public Graphics_buffer {
public:
  explicit Vertex_buffer(std::size_t size);

  /*
private:
  struct Upload_state {
    Staging_buffer staging_buffer{};
    vk::UniqueCommandBuffer command_buffer{};
    vk::UniqueFence fence{};
    std::mutex mutex{};
  };

  vk::UniqueBuffer _buffer{};
  vma::Unique_allocation _allocation{};
  mutable Upload_state _upload_state{};
  */
};
} // namespace fpsparty::client

#endif
