#ifndef FPSPARTY_SERIAL_OSTREAM_WRITER_HPP
#define FPSPARTY_SERIAL_OSTREAM_WRITER_HPP

#include "writer.hpp"
#include <concepts>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace fpsparty::serial {
template <
  typename T,
  typename CharT = T::char_type,
  typename Traits = std::char_traits<CharT>>
  requires std::derived_from<T, std::basic_ostream<CharT, Traits>>
class Ostream_writer : public Writer {
public:
  template <typename... Args>
  explicit Ostream_writer(Args &&...args)
      : _stream{std::forward<Args>(args)...} {}

  virtual void write(std::span<const std::byte> data) override {
    _stream.write(reinterpret_cast<const char *>(data.data()), data.size());
  }

  T const &stream() const noexcept { return _stream; }

  T &stream() noexcept { return _stream; }

private:
  T _stream;
};

using Ofstream_writer = Ostream_writer<std::ofstream>;
using Ostringstream_writer = Ostream_writer<std::ostringstream>;
} // namespace fpsparty::serial

#endif
