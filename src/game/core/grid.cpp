#include "grid.hpp"
#include "serial/serialize.hpp"
#include <cstdint>

namespace fpsparty::game {
Grid::Grid(const Grid_create_info &create_info)
    : _chunk_counts{
          (create_info.width + (Chunk::edge_length - 1)) / Chunk::edge_length,
          (create_info.height + (Chunk::edge_length - 1)) / Chunk::edge_length,
          (create_info.depth + (Chunk::edge_length - 1)) / Chunk::edge_length,
      } {
  _chunks.resize(_chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2]);
}

void Grid::load(serial::Reader &reader) {
  using serial::deserialize;
  const auto x_chunk_count = deserialize<std::uint32_t>(reader);
  if (!x_chunk_count) {
    throw Grid_loading_error{};
  }
  const auto y_chunk_count = deserialize<std::uint32_t>(reader);
  if (!y_chunk_count) {
    throw Grid_loading_error{};
  }
  const auto z_chunk_count = deserialize<std::uint32_t>(reader);
  if (!z_chunk_count) {
    throw Grid_loading_error{};
  }
  _chunk_counts = {*x_chunk_count, *y_chunk_count, *z_chunk_count};
  _chunks.resize(_chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2]);
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
    chunk = Chunk{*x_bits, *y_bits, *z_bits};
  }
}

void Grid::dump(serial::Writer &writer) const {
  using serial::serialize;
  for (auto i = 0; i != 3; ++i) {
    serialize<std::uint32_t>(writer, _chunk_counts[i]);
  }
  for (const auto &chunk : _chunks) {
    for (auto i = 0; i != 3; ++i) {
      serialize<std::uint64_t>(writer, chunk.bits[i]);
    }
  }
}

Chunk_span Grid::get_chunks() noexcept {
  return {_chunks.data(), _chunk_counts};
}

Const_chunk_span Grid::get_chunks() const noexcept {
  return {_chunks.data(), _chunk_counts};
}

Const_chunk_span Grid::get_const_chunks() const noexcept {
  return {_chunks.data(), _chunk_counts};
}

/*
std::size_t
Grid::get_chunk_index(const Eigen::Vector3i &indices) const noexcept {
  const auto j = static_cast<std::size_t>(indices.y() / _chunk_side_length);
  const auto i = static_cast<std::size_t>(indices.x() / _chunk_side_length);
  const auto k = static_cast<std::size_t>(indices.z() / _chunk_side_length);
  return k * _width_chunks * _depth_chunks + j * _width_chunks + i;
}
*/
} // namespace fpsparty::game
