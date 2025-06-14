#ifndef FPSPARTY_SERIAL_WRITER_HPP
#define FPSPARTY_SERIAL_WRITER_HPP

#include <span>

namespace fpsparty::serial {
class Writer {
public:
  virtual ~Writer() = default;

  virtual void write(std::span<std::byte const> data) = 0;
};
} // namespace fpsparty::serial

#endif
