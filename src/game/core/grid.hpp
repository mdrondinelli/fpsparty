#ifndef FPSPARTY_GAME_GRID_HPP
#define FPSPARTY_GAME_GRID_HPP

#include "serial/writer.hpp"
#include <Eigen/Dense>
#include <cstddef>

namespace fpsparty::game {
struct Grid_create_info {
  std::size_t width{};
  std::size_t height{};
  std::size_t depth{};
};

class Grid {
public:
  explicit Grid(const Grid_create_info &create_info);

  void dump(serial::Writer &writer) const;

  bool is_solid(const Eigen::Vector3i &indices) const noexcept;

  void set_solid(const Eigen::Vector3i &indices, bool value) noexcept;

  bool bounds_check(const Eigen::Vector3i &indices) const noexcept;

private:
  static constexpr auto _chunk_side_length = std::size_t{4};

  std::size_t get_chunk_index(const Eigen::Vector3i &indices) const noexcept;

  std::size_t _width_chunks{};
  std::size_t _height_chunks{};
  std::size_t _depth_chunks{};
  std::vector<std::uint64_t> _chunks{};
};
} // namespace fpsparty::game

#endif
