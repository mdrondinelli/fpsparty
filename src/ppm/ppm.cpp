#include "ppm/ppm.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fpsparty::ppm {
namespace {
class Parser {
public:
  explicit Parser(std::span<std::byte const> file) noexcept
      : _file{file}, _offset{} {}

  std::string_view next_token() {
    skip_whitespace_and_comments();
    auto const begin = _offset;
    while (_offset < _file.size() && !is_whitespace(byte_at(_offset))) {
      ++_offset;
    }
    if (begin == _offset) {
      throw std::invalid_argument{"PPM header ended early"};
    }
    return {
      reinterpret_cast<char const *>(_file.data() + begin),
      _offset - begin,
    };
  }

  void consume_required_raster_separator() {
    if (_offset == _file.size() || !is_whitespace(byte_at(_offset))) {
      throw std::invalid_argument{"PPM header is missing raster separator"};
    }
    ++_offset;
  }

  std::span<std::byte const> remaining() const noexcept {
    return _file.subspan(_offset);
  }

private:
  static constexpr bool is_whitespace(unsigned char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f';
  }

  unsigned char byte_at(std::size_t offset) const noexcept {
    return static_cast<unsigned char>(_file[offset]);
  }

  void skip_whitespace_and_comments() noexcept {
    while (_offset < _file.size()) {
      if (is_whitespace(byte_at(_offset))) {
        ++_offset;
      } else if (byte_at(_offset) == '#') {
        while (_offset < _file.size() && byte_at(_offset) != '\n') {
          ++_offset;
        }
      } else {
        return;
      }
    }
  }

  std::span<std::byte const> _file;
  std::size_t _offset;
};

int parse_positive_int(std::string_view token, char const *field_name) {
  auto value = int{};
  auto const *begin = token.data();
  auto const *end = token.data() + token.size();
  auto const [ptr, ec] = std::from_chars(begin, end, value);
  if (ec != std::errc{} || ptr != end || value <= 0) {
    throw std::invalid_argument{
      std::string{"PPM "} + field_name + " must be a positive integer"};
  }
  return value;
}

std::size_t raster_size(int width, int height) {
  auto const max = std::numeric_limits<std::size_t>::max();
  auto const checked_width = static_cast<std::size_t>(width);
  auto const checked_height = static_cast<std::size_t>(height);
  if (checked_width > max / checked_height ||
      checked_width * checked_height > max / 3) {
    throw std::invalid_argument{"PPM dimensions are too large"};
  }
  return checked_width * checked_height * 3;
}
} // namespace

Ppm_image load_ppm(std::span<std::byte const> file) {
  auto parser = Parser{file};
  if (parser.next_token() != "P6") {
    throw std::invalid_argument{"PPM file must be P6"};
  }

  auto const width = parse_positive_int(parser.next_token(), "width");
  auto const height = parse_positive_int(parser.next_token(), "height");
  auto const max_value = parse_positive_int(parser.next_token(), "max value");
  if (max_value != 255) {
    throw std::invalid_argument{"PPM max value must be 255"};
  }
  parser.consume_required_raster_separator();

  auto const data_size = raster_size(width, height);
  auto const raster = parser.remaining();
  if (raster.size() != data_size) {
    throw std::invalid_argument{"PPM raster size does not match dimensions"};
  }

  auto data = std::make_unique<std::byte[]>(data_size);
  std::copy(raster.begin(), raster.end(), data.get());
  return Ppm_image{
    .data = std::move(data),
    .width = width,
    .height = height,
  };
}
} // namespace fpsparty::ppm
