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

void Grid::load(serial::Reader &reader) {
  using serial::deserialize;
  const auto width_chunks = deserialize<std::uint32_t>(reader);
  if (!width_chunks) {
    throw Grid_loading_error{};
  }
  const auto height_chunks = deserialize<std::uint32_t>(reader);
  if (!height_chunks) {
    throw Grid_loading_error{};
  }
  const auto depth_chunks = deserialize<std::uint32_t>(reader);
  if (!depth_chunks) {
    throw Grid_loading_error{};
  }
  _width_chunks = *width_chunks;
  _height_chunks = *height_chunks;
  _depth_chunks = *depth_chunks;
  _chunks.resize(_width_chunks * _height_chunks * _depth_chunks);
  for (auto &chunk : _chunks) {
    const auto chunk_optional = deserialize<std::uint64_t>(reader);
    if (!chunk_optional) {
      throw Grid_loading_error{};
    }
    chunk = *chunk_optional;
  }
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

bool Grid::is_solid(const Eigen::Vector3i &indices) const noexcept {
  if (!bounds_check(indices)) {
    return false;
  }
  const auto chunk_index = get_chunk_index(indices);
  const auto chunk_value = _chunks[chunk_index];
  const auto chunk_relative_x = indices.x() % _chunk_side_length;
  const auto chunk_relative_y = indices.y() % _chunk_side_length;
  const auto chunk_relative_z = indices.z() % _chunk_side_length;
  const auto bit_index =
      chunk_relative_x + chunk_relative_y * _chunk_side_length +
      chunk_relative_z * _chunk_side_length * _chunk_side_length;
  return (chunk_value & (std::uint64_t{1} << bit_index)) != 0;
}

void Grid::set_solid(const Eigen::Vector3i &indices, bool value) noexcept {
  if (!bounds_check(indices)) {
    return;
  }
  const auto chunk_index = get_chunk_index(indices);
  const auto chunk_relative_x = indices.x() % _chunk_side_length;
  const auto chunk_relative_y = indices.y() % _chunk_side_length;
  const auto chunk_relative_z = indices.z() % _chunk_side_length;
  const auto bit_index =
      chunk_relative_x + chunk_relative_y * _chunk_side_length +
      chunk_relative_z * _chunk_side_length * _chunk_side_length;
  if (value) {
    _chunks[chunk_index] |= std::uint64_t{1} << bit_index;
  } else {
    _chunks[chunk_index] &= ~(std::uint64_t{1} << bit_index);
  }
}

bool Grid::bounds_check(const Eigen::Vector3i &indices) const noexcept {
  if (indices.x() < 0) {
    return false;
  }
  if (static_cast<std::size_t>(indices.x()) >=
      _width_chunks * _chunk_side_length) {
    return false;
  }
  if (indices.y() < 0) {
    return false;
  }
  if (static_cast<std::size_t>(indices.y()) >=
      _height_chunks * _chunk_side_length) {
    return false;
  }
  if (indices.z() < 0) {
    return false;
  }
  if (static_cast<std::size_t>(indices.z()) >=
      _depth_chunks * _chunk_side_length) {
    return false;
  }
  return true;
}

std::size_t
Grid::get_chunk_index(const Eigen::Vector3i &indices) const noexcept {
  const auto x = static_cast<std::size_t>(indices.x());
  const auto y = static_cast<std::size_t>(indices.y());
  const auto z = static_cast<std::size_t>(indices.z());
  return x + y * _width_chunks + z * _width_chunks * _height_chunks;
}
} // namespace fpsparty::game
