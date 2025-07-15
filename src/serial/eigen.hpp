#ifndef FPSPARTY_SERIAL_EIGEN_HPP
#define FPSPARTY_SERIAL_EIGEN_HPP

#include "serial/serialize.hpp"
#include <Eigen/Dense>

namespace fpsparty::serial {
template <> struct Serializer<Eigen::Vector2f> {
  void write(Writer &writer, const Eigen::Vector2f &value) const {
    serialize<float>(writer, value.x());
    serialize<float>(writer, value.y());
  }

  std::optional<Eigen::Vector2f> read(Reader &reader) const {
    const auto x = deserialize<float>(reader);
    if (!x) {
      return std::nullopt;
    }
    const auto y = deserialize<float>(reader);
    if (!y) {
      return std::nullopt;
    }
    return Eigen::Vector2f{*x, *y};
  }
};

template <> struct Serializer<Eigen::Vector3f> {
  void write(Writer &writer, const Eigen::Vector3f &value) const {
    serialize<float>(writer, value.x());
    serialize<float>(writer, value.y());
    serialize<float>(writer, value.z());
  }

  std::optional<Eigen::Vector3f> read(Reader &reader) const {
    const auto x = deserialize<float>(reader);
    if (!x) {
      return std::nullopt;
    }
    const auto y = deserialize<float>(reader);
    if (!y) {
      return std::nullopt;
    }
    const auto z = deserialize<float>(reader);
    if (!z) {
      return std::nullopt;
    }
    return Eigen::Vector3f{*x, *y, *z};
  }
};
} // namespace fpsparty::serial

#endif
