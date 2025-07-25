#ifndef FPSPARTY_NET_INPUT_STATE_HPP
#define FPSPARTY_NET_INPUT_STATE_HPP

#include "serial/serialize.hpp"

namespace fpsparty::net {
struct Input_state {
  bool move_left{};
  bool move_right{};
  bool move_forward{};
  bool move_backward{};
  bool use_primary{};
  float yaw{};
  float pitch{};
};
} // namespace fpsparty::net

namespace fpsparty::serial {
template <> struct Serializer<net::Input_state> {
  void write(Writer &writer, const net::Input_state &value) const {
    auto flags = std::uint8_t{};
    if (value.move_left) {
      flags |= 1 << 0;
    }
    if (value.move_right) {
      flags |= 1 << 1;
    }
    if (value.move_forward) {
      flags |= 1 << 2;
    }
    if (value.move_backward) {
      flags |= 1 << 3;
    }
    if (value.use_primary) {
      flags |= 1 << 4;
    }
    serialize<std::uint8_t>(writer, flags);
    serialize<float>(writer, value.yaw);
    serialize<float>(writer, value.pitch);
  }

  std::optional<net::Input_state> read(Reader &reader) const {
    const auto flags = deserialize<std::uint8_t>(reader);
    if (!flags) {
      return std::nullopt;
    }
    const auto yaw = deserialize<float>(reader);
    if (!yaw) {
      return std::nullopt;
    }
    const auto pitch = deserialize<float>(reader);
    if (!pitch) {
      return std::nullopt;
    }
    return net::Input_state{
        .move_left = (*flags & (1 << 0)) != 0,
        .move_right = (*flags & (1 << 1)) != 0,
        .move_forward = (*flags & (1 << 2)) != 0,
        .move_backward = (*flags & (1 << 3)) != 0,
        .use_primary = (*flags & (1 << 4)) != 0,
        .yaw = *yaw,
        .pitch = *pitch,
    };
  }
};
} // namespace fpsparty::serial
#endif
