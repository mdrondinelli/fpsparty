#include "grid.hpp"
#include "serial/serialize.hpp"
#include <cstdint>

namespace fpsparty::game {
Grid::Grid(const Grid_create_info &create_info)
    : _width_chunks{(create_info.width + (_chunk_side_length - 1)) /
                    _chunk_side_length},
      _height_chunks{(create_info.height + (_chunk_side_length - 1)) /
                     _chunk_side_length},
      _depth_chunks{(create_info.depth + (_chunk_side_length - 1)) /
                    _chunk_side_length} {
  _chunks.resize(_width_chunks * _height_chunks * _depth_chunks);
}

void Grid::dump(serial::Writer &writer) const {
  using serial::serialize;
  serialize<std::uint32_t>(writer, _width_chunks);
  serialize<std::uint32_t>(writer, _height_chunks);
  serialize<std::uint32_t>(writer, _depth_chunks);
  for (const auto &chunk : _chunks) {
    serialize<std::uint64_t>(writer, chunk);
  }
}

bool Grid::is_solid(const Eigen::Vector3i &location) const noexcept {
  if (!bounds_check(location)) {
    return false;
  }
  const auto chunk_index = get_chunk_index(location);
  const auto chunk_value = _chunks[chunk_index];
  const auto chunk_relative_x = location.x() % _chunk_side_length;
  const auto chunk_relative_y = location.y() % _chunk_side_length;
  const auto chunk_relative_z = location.z() % _chunk_side_length;
  const auto bit_index =
      chunk_relative_x + chunk_relative_y * _chunk_side_length +
      chunk_relative_z * _chunk_side_length * _chunk_side_length;
  return (chunk_value & (std::uint64_t{1} << bit_index)) != 0;
}

void Grid::set_solid(const Eigen::Vector3i &location, bool value) noexcept {
  if (!bounds_check(location)) {
    return;
  }
  const auto chunk_index = get_chunk_index(location);
  const auto chunk_relative_x = location.x() % _chunk_side_length;
  const auto chunk_relative_y = location.y() % _chunk_side_length;
  const auto chunk_relative_z = location.z() % _chunk_side_length;
  const auto bit_index =
      chunk_relative_x + chunk_relative_y * _chunk_side_length +
      chunk_relative_z * _chunk_side_length * _chunk_side_length;
  if (value) {
    _chunks[chunk_index] |= std::uint64_t{1} << bit_index;
  } else {
    _chunks[chunk_index] &= ~(std::uint64_t{1} << bit_index);
  }
}

bool Grid::bounds_check(const Eigen::Vector3i &location) const noexcept {
  if (location.x() < 0) {
    return false;
  }
  if (static_cast<std::size_t>(location.x()) >=
      _width_chunks * _chunk_side_length) {
    return false;
  }
  if (location.y() < 0) {
    return false;
  }
  if (static_cast<std::size_t>(location.y()) >=
      _height_chunks * _chunk_side_length) {
    return false;
  }
  if (location.z() < 0) {
    return false;
  }
  if (static_cast<std::size_t>(location.z()) >=
      _depth_chunks * _chunk_side_length) {
    return false;
  }
  return true;
}

std::size_t
Grid::get_chunk_index(const Eigen::Vector3i &location) const noexcept {
  const auto x = static_cast<std::size_t>(location.x());
  const auto y = static_cast<std::size_t>(location.y());
  const auto z = static_cast<std::size_t>(location.z());
  return x + y * _width_chunks + z * _width_chunks * _height_chunks;
}
} // namespace fpsparty::game
