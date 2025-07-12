#ifndef FPSPARTY_SERIAL_SPAN_READER_HPP
#define FPSPARTY_SERIAL_SPAN_READER_HPP

#include "serial/reader.hpp"
#include <cstddef>
#include <cstring>
#include <optional>
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

  std::optional<Span_reader>
  subspan_reader(std::size_t offset,
                 std::size_t count = std::dynamic_extent) const noexcept {
    if (offset < _data.size() &&
        (count == std::dynamic_extent || count <= _data.size() - offset)) {
      return subspan_reader_unchecked(offset, count);
    } else {
      return std::nullopt;
    }
  }

  Span_reader subspan_reader_unchecked(
      std::size_t offset,
      std::size_t count = std::dynamic_extent) const noexcept {
    return Span_reader{_data.subspan(offset, count)};
  }

  std::span<const std::byte> data() const noexcept { return _data; }

  std::size_t offset() const noexcept { return _offset; }

private:
  std::span<const std::byte> _data{};
  std::size_t _offset{};
};
} // namespace fpsparty::serial

#endif
