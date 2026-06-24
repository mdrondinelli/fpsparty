#ifndef FPSPARTY_GAME_BLOCK_HPP
#define FPSPARTY_GAME_BLOCK_HPP

#include <int.hpp>
#include <utility>

namespace fpsparty::game {

enum class Block { air, placeholder, stone, dirt, conveyor };

constexpr u16 pack_block_data(Block block, u8 data) noexcept {
  return ((data & 0xff) << 8) | static_cast<u8>(block);
}

constexpr std::pair<Block, u8> unpack_block_data(u16 packed) noexcept {
  return {
    static_cast<Block>(packed & 0xff),
    static_cast<u8>(packed >> 8),
  };
}

} // namespace fpsparty::game

#endif
