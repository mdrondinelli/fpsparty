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
    const auto x_bits = deserialize<std::uint64_t>(reader);
    if (!x_bits) {
      throw Grid_loading_error{};
    }
    const auto y_bits = deserialize<std::uint64_t>(reader);
    if (!y_bits) {
      throw Grid_loading_error{};
    }
    const auto z_bits = deserialize<std::uint64_t>(reader);
    if (!z_bits) {
      throw Grid_loading_error{};
    }
    chunk = Chunk{.x_bits = *x_bits, .y_bits = *y_bits, .z_bits = *z_bits};
  }
}

void Grid::dump(serial::Writer &writer) const {
  using serial::serialize;
  serialize<std::uint32_t>(writer, _width_chunks);
  serialize<std::uint32_t>(writer, _height_chunks);
  serialize<std::uint32_t>(writer, _depth_chunks);
  for (const auto &chunk : _chunks) {
    serialize<std::uint64_t>(writer, chunk.x_bits);
    serialize<std::uint64_t>(writer, chunk.y_bits);
    serialize<std::uint64_t>(writer, chunk.z_bits);
  }
}

bool Grid::is_x_plane_solid(const Eigen::Vector3i &indices) const noexcept {
  if (!bounds_check(indices)) {
    return false;
  }
  const auto chunk_index = get_chunk_index(indices);
  const auto &chunk = _chunks[chunk_index];
  const auto bit_index = get_bit_index(indices);
  return chunk.x_bits & (std::uint64_t{1} << bit_index);
}

bool Grid::is_y_plane_solid(const Eigen::Vector3i &indices) const noexcept {
  if (!bounds_check(indices)) {
    return false;
  }
  const auto chunk_index = get_chunk_index(indices);
  const auto &chunk = _chunks[chunk_index];
  const auto bit_index = get_bit_index(indices);
  return chunk.y_bits & (std::uint64_t{1} << bit_index);
}

bool Grid::is_z_plane_solid(const Eigen::Vector3i &indices) const noexcept {
  if (!bounds_check(indices)) {
    return false;
  }
  const auto chunk_index = get_chunk_index(indices);
  const auto &chunk = _chunks[chunk_index];
  const auto bit_index = get_bit_index(indices);
  return chunk.z_bits & (std::uint64_t{1} << bit_index);
}

void Grid::set_x_plane_solid(const Eigen::Vector3i &indices,
                             bool value) noexcept {
  if (!bounds_check(indices)) {
    return;
  }
  const auto chunk_index = get_chunk_index(indices);
  auto &chunk = _chunks[chunk_index];
  const auto bit_index = get_bit_index(indices);
  if (value) {
    chunk.x_bits |= (std::uint64_t{1} << bit_index);
  } else {
    chunk.x_bits &= ~(std::uint64_t{1} << bit_index);
  }
}

void Grid::set_y_plane_solid(const Eigen::Vector3i &indices,
                             bool value) noexcept {
  if (!bounds_check(indices)) {
    return;
  }
  const auto chunk_index = get_chunk_index(indices);
  auto &chunk = _chunks[chunk_index];
  const auto bit_index = get_bit_index(indices);
  if (value) {
    chunk.y_bits |= (std::uint64_t{1} << bit_index);
  } else {
    chunk.y_bits &= ~(std::uint64_t{1} << bit_index);
  }
}

void Grid::set_z_plane_solid(const Eigen::Vector3i &indices,
                             bool value) noexcept {
  if (!bounds_check(indices)) {
    return;
  }
  const auto chunk_index = get_chunk_index(indices);
  auto &chunk = _chunks[chunk_index];
  const auto bit_index = get_bit_index(indices);
  if (value) {
    chunk.z_bits |= (std::uint64_t{1} << bit_index);
  } else {
    chunk.z_bits &= ~(std::uint64_t{1} << bit_index);
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
  const auto j = static_cast<std::size_t>(indices.y() / _chunk_side_length);
  const auto i = static_cast<std::size_t>(indices.x() / _chunk_side_length);
  const auto k = static_cast<std::size_t>(indices.z() / _chunk_side_length);
  return k * _width_chunks * _depth_chunks + j * _width_chunks + i;
}
} // namespace fpsparty::game
