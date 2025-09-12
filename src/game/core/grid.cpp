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
  assert(_chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2] ==
         _chunks.size());
  return {_chunks.data(), _chunk_counts};
}

Const_chunk_span Grid::get_chunks() const noexcept {
  assert(_chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2] ==
         _chunks.size());
  return {_chunks.data(), _chunk_counts};
}

Const_chunk_span Grid::get_const_chunks() const noexcept {
  assert(_chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2] ==
         _chunks.size());
  return {_chunks.data(), _chunk_counts};
}

void Grid::fill(Axis normal, int layer, const Eigen::AlignedBox2i &bounds) {
  // TODO: optimize this to take advantage of bitwise ops
  if (layer < 0) {
    return;
  }
  switch (normal) {
  case Axis::x: {
    const auto x = layer;
    const auto i = x / Chunk::edge_length;
    const auto x_0 = static_cast<int>(i * Chunk::edge_length);
    if (i >= _chunk_counts[0]) {
      return;
    }
    const auto min_y = std::max(0, bounds.min().x());
    const auto min_z = std::max(0, bounds.min().y());
    const auto max_y =
        std::min(static_cast<int>(get_height()), bounds.max().x());
    const auto max_z =
        std::min(static_cast<int>(get_depth()), bounds.max().y());
    for (auto z = min_z; z != max_z; ++z) {
      const auto k = z / Chunk::edge_length;
      const auto z_0 = static_cast<int>(k * Chunk::edge_length);
      for (auto y = min_y; y != max_y; ++y) {
        const auto j = y / Chunk::edge_length;
        const auto y_0 = static_cast<int>(j * Chunk::edge_length);
        const auto chunk_index =
            detail::linearize_chunk_offset(_chunk_counts, {i, j, k});
        auto &chunk = _chunks[chunk_index];
        chunk.set_solid(normal, {x - x_0, y - y_0, z - z_0}, true);
      }
    }
    break;
  }
  case Axis::y: {
    const auto y = layer;
    const auto j = y / Chunk::edge_length;
    const auto y_0 = static_cast<int>(j * Chunk::edge_length);
    if (j >= _chunk_counts[1]) {
      return;
    }
    const auto min_x = std::max(0, bounds.min().x());
    const auto min_z = std::max(0, bounds.min().y());
    const auto max_x =
        std::min(static_cast<int>(get_width()), bounds.max().x());
    const auto max_z =
        std::min(static_cast<int>(get_depth()), bounds.max().y());
    for (auto z = min_z; z != max_z; ++z) {
      const auto k = z / Chunk::edge_length;
      const auto z_0 = static_cast<int>(k * Chunk::edge_length);
      for (auto x = min_x; x != max_x; ++x) {
        const auto i = x / Chunk::edge_length;
        const auto x_0 = static_cast<int>(i * Chunk::edge_length);
        const auto chunk_index =
            detail::linearize_chunk_offset(_chunk_counts, {i, j, k});
        auto &chunk = _chunks[chunk_index];
        chunk.set_solid(normal, {x - x_0, y - y_0, z - z_0}, true);
      }
    }
    break;
  }
  case Axis::z: {
    const auto z = layer;
    const auto k = z / Chunk::edge_length;
    const auto z_0 = static_cast<int>(k * Chunk::edge_length);
    if (k >= _chunk_counts[2]) {
      return;
    }
    const auto min_x = std::max(0, bounds.min().x());
    const auto min_y = std::max(0, bounds.min().y());
    const auto max_x =
        std::min(static_cast<int>(get_width()), bounds.max().x());
    const auto max_y =
        std::min(static_cast<int>(get_height()), bounds.max().y());
    for (auto y = min_y; y != max_y; ++y) {
      const auto j = y / Chunk::edge_length;
      const auto y_0 = static_cast<int>(j * Chunk::edge_length);
      for (auto x = min_x; x != max_x; ++x) {
        const auto i = x / Chunk::edge_length;
        const auto x_0 = static_cast<int>(i * Chunk::edge_length);
        const auto chunk_index =
            detail::linearize_chunk_offset(_chunk_counts, {i, j, k});
        auto &chunk = _chunks[chunk_index];
        chunk.set_solid(normal, {x - x_0, y - y_0, z - z_0}, true);
      }
    }
    break;
  }
  }
}

std::size_t Grid::get_width() const noexcept {
  return _chunk_counts[0] * Chunk::edge_length;
}

std::size_t Grid::get_height() const noexcept {
  return _chunk_counts[1] * Chunk::edge_length;
}

std::size_t Grid::get_depth() const noexcept {
  return _chunk_counts[2] * Chunk::edge_length;
}
} // namespace fpsparty::game
