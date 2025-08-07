#ifndef FPSPARTY_GAME_CORE_GRID_HPP
#define FPSPARTY_GAME_CORE_GRID_HPP

#include "serial/reader.hpp"
#include "serial/writer.hpp"
#include <Eigen/Dense>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>

namespace fpsparty::game {
enum class Axis { x, y, z };

struct Grid_create_info {
  std::size_t width{};
  std::size_t height{};
  std::size_t depth{};
};

class Grid_loading_error : public std::exception {};

class Chunk {
public:
  static constexpr auto edge_length = std::size_t{4};

  static constexpr auto get_bit_index(const Eigen::Vector3i &offsets) noexcept {
    const auto i = offsets.x() % edge_length;
    const auto j = offsets.y() % edge_length;
    const auto k = offsets.z() % edge_length;
    return k * edge_length * edge_length + j * edge_length + i;
  }

  Chunk() noexcept = default;

  constexpr Chunk(std::uint64_t x_bits, std::uint64_t y_bits,
                  std::uint64_t z_bits) noexcept
      : bits{x_bits, y_bits, z_bits} {}

  constexpr bool is_solid(Axis axis,
                          const Eigen::Vector3i &offsets) const noexcept {
    const auto bit_index = get_bit_index(offsets);
    return bits[static_cast<int>(axis)] & (std::uint64_t{1} << bit_index);
  }

  constexpr void set_solid(Axis axis, const Eigen::Vector3i &offsets,
                           bool value) noexcept {
    const auto bit_index = get_bit_index(offsets);
    if (value) {
      bits[static_cast<int>(axis)] |= (std::uint64_t{1} << bit_index);
    } else {
      bits[static_cast<int>(axis)] &= ~(std::uint64_t{1} << bit_index);
    }
  }

  std::array<std::uint64_t, 3> bits;
};

class Grid {
public:
  explicit Grid(const Grid_create_info &create_info);

  void load(serial::Reader &reader);

  void dump(serial::Writer &writer) const;

private:
  // std::size_t get_chunk_index(const Eigen::Vector3i &indices) const noexcept;

  std::array<std::size_t, 3> _axial_chunk_counts;
  std::vector<Chunk> _chunks{};
};
} // namespace fpsparty::game

#endif
