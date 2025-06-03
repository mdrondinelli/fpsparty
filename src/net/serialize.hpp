#ifndef FPSPARTY_NET_SERIALIZE_HPP
#define FPSPARTY_NET_SERIALIZE_HPP

#include "writer.hpp"
#include <bit>
#include <exception>
#include <span>

namespace fpsparty::net {
static_assert(std::endian::native == std::endian::little ||
              std::endian::native == std::endian::big);

class Serialization_error : public std::exception {};

template <typename T> struct Serialize;

template <> struct Serialize<std::uint8_t> {
  template <std::integral T> void operator()(Writer &writer, T value) const {
    if (value < 0 || value > std::numeric_limits<std::uint8_t>::max()) {
      throw Serialization_error{};
    }
    const auto casted_value = static_cast<std::uint8_t>(value);
    writer.write(std::as_bytes(std::span{&casted_value, 1}));
  }
};

template <> struct Serialize<float> {
  void operator()(Writer &writer, float value) const {
    const auto casted_value = std::bit_cast<std::uint32_t>(value);
    const auto byteswapped_value = std::endian::native == std::endian::little
                                       ? std::byteswap(casted_value)
                                       : casted_value;
    writer.write(std::as_bytes(std::span{&byteswapped_value, 1}));
  }
};

template <typename T, typename U>
void serialize(Writer &writer, const U &value) {
  Serialize<T>{}(writer, value);
}
} // namespace fpsparty::net

#endif
