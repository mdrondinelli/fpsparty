#ifndef FPSPARTY_SERIAL_EIGEN_HPP
#define FPSPARTY_SERIAL_EIGEN_HPP

#include <math/box.hpp>
#include <math/vec.hpp>

#include "serialize.hpp"

namespace fpsparty::serial {
template <> struct Serializer<math::vec2> {
  void write(Writer &writer, math::vec2 const &value) const {
    serialize<float>(writer, value.x());
    serialize<float>(writer, value.y());
  }

  std::optional<math::vec2> read(Reader &reader) const {
    auto const x = deserialize<float>(reader);
    if (!x) {
      return std::nullopt;
    }
    auto const y = deserialize<float>(reader);
    if (!y) {
      return std::nullopt;
    }
    return math::vec2{*x, *y};
  }
};

template <> struct Serializer<math::vec3> {
  void write(Writer &writer, math::vec3 const &value) const {
    serialize<float>(writer, value.x());
    serialize<float>(writer, value.y());
    serialize<float>(writer, value.z());
  }

  std::optional<math::vec3> read(Reader &reader) const {
    auto const x = deserialize<float>(reader);
    if (!x) {
      return std::nullopt;
    }
    auto const y = deserialize<float>(reader);
    if (!y) {
      return std::nullopt;
    }
    auto const z = deserialize<float>(reader);
    if (!z) {
      return std::nullopt;
    }
    return math::vec3{*x, *y, *z};
  }
};

template <> struct Serializer<math::ibox3> {
  void write(Writer &writer, math::ibox3 const &value) const {
    serialize<i32>(writer, value.min().x());
    serialize<i32>(writer, value.min().y());
    serialize<i32>(writer, value.min().z());
    serialize<i32>(writer, value.max().x());
    serialize<i32>(writer, value.max().y());
    serialize<i32>(writer, value.max().z());
  }

  std::optional<math::ibox3> read(Reader &reader) const {
    auto const min_x = deserialize<i32>(reader);
    if (!min_x) {
      return std::nullopt;
    }
    auto const min_y = deserialize<i32>(reader);
    if (!min_y) {
      return std::nullopt;
    }
    auto const min_z = deserialize<i32>(reader);
    if (!min_z) {
      return std::nullopt;
    }
    auto const max_x = deserialize<i32>(reader);
    if (!max_x) {
      return std::nullopt;
    }
    auto const max_y = deserialize<i32>(reader);
    if (!max_y) {
      return std::nullopt;
    }
    auto const max_z = deserialize<i32>(reader);
    if (!max_z) {
      return std::nullopt;
    }
    return math::ibox3{
      math::ivec3{*min_x, *min_y, *min_z},
      math::ivec3{*max_x, *max_y, *max_z},
    };
  }
};
} // namespace fpsparty::serial

#endif
