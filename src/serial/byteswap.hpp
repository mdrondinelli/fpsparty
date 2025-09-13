#ifndef FPSPARTY_SERIAL_BYTESWAP_HPP
#define FPSPARTY_SERIAL_BYTESWAP_HPP

#include <bit>

namespace fpsparty::serial {
static_assert(
  std::endian::native == std::endian::little ||
  std::endian::native == std::endian::big
);

template <typename T> constexpr T network_byteswap(T value) {
  return std::endian::native == std::endian::little ? std::byteswap(value)
                                                    : value;
}
} // namespace fpsparty::serial

#endif
