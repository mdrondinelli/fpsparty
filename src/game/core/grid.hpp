#ifndef FPSPARTY_GAME_CORE_GRID_HPP
#define FPSPARTY_GAME_CORE_GRID_HPP

#include "serial/reader.hpp"
#include "serial/writer.hpp"
#include <Eigen/Dense>
#include <cstddef>
#include <exception>

namespace fpsparty::game {
struct Grid_create_info {
  std::size_t width{};
  std::size_t height{};
  std::size_t depth{};
};

class Grid_loading_error : public std::exception {};

class Grid {
public:
  explicit Grid(const Grid_create_info &create_info);

  void load(serial::Reader &reader);

  void dump(serial::Writer &writer) const;

  bool is_x_plane_solid(const Eigen::Vector3i &indices) const noexcept;

  bool is_y_plane_solid(const Eigen::Vector3i &indices) const noexcept;

  bool is_z_plane_solid(const Eigen::Vector3i &indices) const noexcept;

  void set_x_plane_solid(const Eigen::Vector3i &indices, bool value) noexcept;

  void set_y_plane_solid(const Eigen::Vector3i &indices, bool value) noexcept;

  void set_z_plane_solid(const Eigen::Vector3i &indices, bool value) noexcept;

  bool bounds_check(const Eigen::Vector3i &indices) const noexcept;

private:
  struct Chunk {
    std::uint64_t x_bits;
    std::uint64_t y_bits;
    std::uint64_t z_bits;
  };

  static constexpr auto _chunk_side_length = std::size_t{4};

  static constexpr auto get_bit_index(const Eigen::Vector3i &indices) noexcept {
    const auto i = indices.x() % _chunk_side_length;
    const auto j = indices.y() % _chunk_side_length;
    const auto k = indices.z() % _chunk_side_length;
    return k * _chunk_side_length * _chunk_side_length +
           j * _chunk_side_length + i;
  }

  std::size_t get_chunk_index(const Eigen::Vector3i &indices) const noexcept;

  std::size_t _width_chunks{};
  std::size_t _height_chunks{};
  std::size_t _depth_chunks{};
  std::vector<Chunk> _chunks{};
};
} // namespace fpsparty::game

#endif
