#ifndef FPSPARTY_GAME_BLOCK_HPP
#define FPSPARTY_GAME_BLOCK_HPP

#include <cstddef>
#include <utility>

namespace fpsparty::game {

enum class Block { air, placeholder, stone, dirt, conveyor };

constexpr std::byte pack_block_data(Block block, int data) noexcept {
  return (static_cast<std::byte>(block) << 2) |
         static_cast<std::byte>(data & 0b11);
}

constexpr std::pair<Block, int> unpack_block_data(std::byte packed) noexcept {
  return {
    static_cast<Block>(packed >> 2),
    static_cast<int>(packed & std::byte{0b11}),
  };
}

} // namespace fpsparty::game

#endif
