#ifndef FPSPARTY_NET_INPUT_STATE_HPP
#define FPSPARTY_NET_INPUT_STATE_HPP

#include <int.hpp>

#include <serial/serialize.hpp>

namespace fpsparty::net {

struct Input_state {
  bool move_left{};
  bool move_right{};
  bool move_forward{};
  bool move_backward{};
  bool jump{};
  bool run{};
  bool use_primary{};
  bool use_secondary{};
  bool drop{};
  u8 slot_index{};
  float yaw{};
  float pitch{};
};
} // namespace fpsparty::net

namespace fpsparty::serial {
template <> struct Serializer<net::Input_state> {
  void write(Writer &writer, net::Input_state const &value) const {
    auto flags = u16{};
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
    if (value.jump) {
      flags |= 1 << 4;
    }
    if (value.run) {
      flags |= 1 << 5;
    }
    if (value.use_primary) {
      flags |= 1 << 6;
    }
    if (value.use_secondary) {
      flags |= 1 << 7;
    }
    if (value.drop) {
      flags |= 1 << 8;
    }
    serialize<u16>(writer, flags);
    serialize<u8>(writer, value.slot_index);
    serialize<float>(writer, value.yaw);
    serialize<float>(writer, value.pitch);
  }

  std::optional<net::Input_state> read(Reader &reader) const {
    auto const flags = deserialize<std::uint16_t>(reader);
    if (!flags) {
      return std::nullopt;
    }
    auto const slot_index = deserialize<std::uint8_t>(reader);
    if (!slot_index) {
      return std::nullopt;
    }
    auto const yaw = deserialize<float>(reader);
    if (!yaw) {
      return std::nullopt;
    }
    auto const pitch = deserialize<float>(reader);
    if (!pitch) {
      return std::nullopt;
    }
    return net::Input_state{
      .move_left = (*flags & (1 << 0)) != 0,
      .move_right = (*flags & (1 << 1)) != 0,
      .move_forward = (*flags & (1 << 2)) != 0,
      .move_backward = (*flags & (1 << 3)) != 0,
      .jump = (*flags & (1 << 4)) != 0,
      .run = (*flags & (1 << 5)) != 0,
      .use_primary = (*flags & (1 << 6)) != 0,
      .use_secondary = (*flags & (1 << 7)) != 0,
      .drop = (*flags & (1 << 8)) != 0,
      .slot_index = *slot_index,
      .yaw = *yaw,
      .pitch = *pitch,
    };
  }
};

} // namespace fpsparty::serial

#endif
