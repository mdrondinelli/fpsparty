#ifndef FPSPARTY_SERIAL_SPAN_READER_HPP
#define FPSPARTY_SERIAL_SPAN_READER_HPP

#include "serial/reader.hpp"
#include <cstddef>
#include <cstring>
#include <span>

namespace fpsparty::serial {
class Span_reader : public Reader {
public:
  constexpr Span_reader() noexcept = default;

  constexpr explicit Span_reader(std::span<const std::byte> data) noexcept
      : _data{data}, _offset{} {}

  virtual bool read(std::span<std::byte> data) override {
    if (_offset + data.size() <= _data.size()) {
      std::memcpy(data.data(), _data.data() + _offset, data.size());
      _offset += data.size();
      return true;
    } else {
      return false;
    }
  }

  std::span<const std::byte> data() const noexcept { return _data; }

  std::size_t offset() const noexcept { return _offset; }

private:
  std::span<const std::byte> _data{};
  std::size_t _offset{};
};
} // namespace fpsparty::serial

#endif
