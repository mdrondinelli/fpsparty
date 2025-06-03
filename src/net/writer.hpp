#ifndef FPSPARTY_NET_WRITER_HPP
#define FPSPARTY_NET_WRITER_HPP

#include <span>

namespace fpsparty::net {
class Writer {
public:
  virtual ~Writer() = default;

  virtual void write(std::span<const std::byte> data) = 0;
};
} // namespace fpsparty::net

#endif
