#ifndef FPSPARTY_SERIAL_SPAN_WRITER_HPP
#define FPSPARTY_SERIAL_SPAN_WRITER_HPP

#include "serial/write_error.hpp"
#include "serial/writer.hpp"
#include <cstddef>
#include <cstring>
#include <span>

namespace fpsparty::serial {
class Span_writer_overflow_error : public Write_error {};

class Span_writer : public Writer {
public:
  constexpr Span_writer() noexcept = default;

  constexpr explicit Span_writer(std::span<std::byte> data) noexcept
      : _data{data}, _offset{} {}

  void write(std::span<std::byte const> data) override {
    if (_offset + data.size() <= _data.size()) {
      std::memcpy(_data.data() + _offset, data.data(), data.size());
      _offset += data.size();
    } else {
      throw Span_writer_overflow_error{};
    }
  }

  std::size_t offset() const noexcept { return _offset; }

private:
  std::span<std::byte> _data{};
  std::size_t _offset{};
};
} // namespace fpsparty::serial

#endif
