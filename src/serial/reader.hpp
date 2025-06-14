#ifndef FPSPARTY_SERIAL_READER_HPP
#define FPSPARTY_SERIAL_READER_HPP

#include <cstddef>
#include <span>

namespace fpsparty::serial {
class Reader {
public:
  virtual ~Reader() = default;

  [[nodiscard]] virtual bool read(std::span<std::byte> data) = 0;
};
} // namespace fpsparty::serial

#endif
