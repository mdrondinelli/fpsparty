#ifndef FPSPARTY_SERIAL_SERIALIZE_HPP
#define FPSPARTY_SERIAL_SERIALIZE_HPP

#include "serial/byteswap.hpp"
#include "serial/reader.hpp"
#include "serial/writer.hpp"
#include <algorithm>
#include <bit>
#include <exception>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <span>
#include <type_traits>

namespace fpsparty::serial {
class Serialization_error : public std::exception {};

template <typename T> struct Serializer;

template <> struct Serializer<bool> {
  void write(Writer &writer, bool value) const {
    const auto int_value = value ? std::uint8_t{1} : std::uint8_t{0};
    writer.write(std::as_bytes(std::span{&int_value, 1}));
  }

  std::optional<bool> read(Reader &reader) const {
    auto int_value = std::uint8_t{};
    if (!reader.read(std::as_writable_bytes(std::span{&int_value, 1}))) {
      return std::nullopt;
    }
    return int_value ? true : false;
  }
};

template <std::integral T> struct Serializer<T> {
  template <std::integral U> void write(Writer &writer, U value) const {
    if (value < std::numeric_limits<T>::min() ||
        value > std::numeric_limits<T>::max()) {
      throw Serialization_error{};
    }
    const auto casted_value = static_cast<T>(value);
    const auto byteswapped_value = network_byteswap(casted_value);
    writer.write(std::as_bytes(std::span{&byteswapped_value, 1}));
  }

  std::optional<T> read(Reader &reader) const {
    auto value = T{};
    if (!reader.read(std::as_writable_bytes(std::span{&value, 1}))) {
      return std::nullopt;
    }
    value = network_byteswap(value);
    return value;
  }
};

template <> struct Serializer<float> {
  void write(Writer &writer, float value) const {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    const auto casted_value = std::bit_cast<std::uint32_t>(value);
    const auto byteswapped_value = network_byteswap(casted_value);
    writer.write(std::as_bytes(std::span{&byteswapped_value, 1}));
  }

  std::optional<float> read(Reader &reader) const {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    auto buffer = std::uint32_t{};
    if (!reader.read(std::as_writable_bytes(std::span{&buffer, 1}))) {
      return std::nullopt;
    }
    buffer = network_byteswap(buffer);
    return std::bit_cast<float>(buffer);
  }
};

template <typename T>
  requires std::is_enum_v<T>
struct Serializer<T> {
  void write(Writer &writer, T value) const {
    const auto casted_value = std::bit_cast<std::underlying_type_t<T>>(value);
    const auto byteswapped_value = network_byteswap(casted_value);
    writer.write(std::as_bytes(std::span{&byteswapped_value, 1}));
  }

  std::optional<T> read(Reader &reader) const {
    auto buffer = std::underlying_type_t<T>{};
    if (!reader.read(std::as_writable_bytes(std::span{&buffer, 1}))) {
      return std::nullopt;
    }
    network_byteswap(buffer);
    const auto enum_value = static_cast<T>(buffer);
    const auto enum_values = magic_enum::enum_values<T>();
    const auto enum_value_it = std::ranges::find(enum_values, enum_value);
    if (enum_value_it == enum_values.end()) {
      return std::nullopt;
    }
    return enum_value;
  }
};

template <typename T> struct Serializer<std::optional<T>> {
  void write(Writer &writer, const std::optional<T> &value) const {
    Serializer<bool>{}.write(writer, value.has_value());
    if (value) {
      Serializer<T>{}.write(writer, *value);
    }
  }

  std::optional<std::optional<T>> read(Reader &reader) const {
    const auto has_value = Serializer<bool>{}.read(reader);
    if (!has_value) {
      return std::nullopt;
    }
    if (*has_value) {
      return Serializer<T>{}.read(reader);
    } else {
      return std::optional{std::optional<T>{}};
    }
  }
};

template <typename T, typename U>
void serialize(Writer &writer, const U &value) {
  Serializer<T>{}.write(writer, value);
}

template <typename T> std::optional<T> deserialize(Reader &reader) {
  return Serializer<T>{}.read(reader);
}
} // namespace fpsparty::serial

#ifdef FPSPARTY_SERIAL_EIGEN
#include "serial/eigen.hpp"
#endif

#endif
