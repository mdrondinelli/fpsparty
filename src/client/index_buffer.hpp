#ifndef FPSPARTY_CLIENT_INDEX_BUFFER_HPP
#define FPSPARTY_CLIENT_INDEX_BUFFER_HPP

#include "client/graphics_buffer.hpp"
#include "rc.hpp"

namespace fpsparty::client {
class Index_buffer : public rc::Object<Index_buffer>, public Graphics_buffer {
public:
  explicit Index_buffer(std::size_t size);

  /*
private:
  struct Upload_state {
    Staging_buffer staging_buffer{};
    vk::UniqueCommandBuffer command_buffer{};
    vk::UniqueFence fence{};
    std::mutex mutex{};
  };
  */
};
} // namespace fpsparty::client

#endif
