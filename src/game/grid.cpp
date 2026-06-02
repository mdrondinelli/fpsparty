#include "grid.hpp"
#include "serial/serialize.hpp"
#include <cassert>
#include <cstdint>
#include <limits>

namespace fpsparty::game {
Grid::Grid(Grid_create_info const &create_info)
    : _chunk_counts{
        (create_info.width + (Chunk::edge_length - 1)) / Chunk::edge_length,
        (create_info.height + (Chunk::edge_length - 1)) / Chunk::edge_length,
        (create_info.depth + (Chunk::edge_length - 1)) / Chunk::edge_length,
      } {
  _chunks.resize(_chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2]);
}

void Grid::load(serial::Reader &reader) {
  using serial::deserialize;
  auto const x_chunk_count = deserialize<std::uint32_t>(reader);
  if (!x_chunk_count) {
    throw Grid_loading_error{};
  }
  auto const y_chunk_count = deserialize<std::uint32_t>(reader);
  if (!y_chunk_count) {
    throw Grid_loading_error{};
  }
  auto const z_chunk_count = deserialize<std::uint32_t>(reader);
  if (!z_chunk_count) {
    throw Grid_loading_error{};
  }
  _chunk_counts = {*x_chunk_count, *y_chunk_count, *z_chunk_count};
  _chunks.resize(_chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2]);
  for (auto &chunk : _chunks) {
    auto const blocks = deserialize<std::uint64_t>(reader);
    if (!blocks) {
      throw Grid_loading_error{};
    }
    chunk = Chunk{*blocks};
  }
}

void Grid::dump(serial::Writer &writer) const {
  using serial::serialize;
  for (auto i = 0; i != 3; ++i) {
    serialize<std::uint32_t>(writer, _chunk_counts[i]);
  }
  for (auto const &chunk : _chunks) {
    serialize<std::uint64_t>(writer, chunk.blocks);
  }
}

Chunk_span Grid::get_chunks() noexcept {
  assert(
    _chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2] == _chunks.size());
  return {_chunks.data(), _chunk_counts};
}

Const_chunk_span Grid::get_chunks() const noexcept {
  assert(
    _chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2] == _chunks.size());
  return {_chunks.data(), _chunk_counts};
}

Const_chunk_span Grid::get_const_chunks() const noexcept {
  assert(
    _chunk_counts[0] * _chunk_counts[1] * _chunk_counts[2] == _chunks.size());
  return {_chunks.data(), _chunk_counts};
}

void Grid::fill(Eigen::AlignedBox3i const &bounds, bool solid) {
  // TODO: optimize this to take advantage of bitwise ops
  auto const min = bounds.min().cwiseMax(Eigen::Vector3i::Zero()).eval();
  auto const max = bounds.max()
                     .cwiseMin(
                       Eigen::Vector3i{
                         static_cast<int>(get_width()),
                         static_cast<int>(get_height()),
                         static_cast<int>(get_depth()),
                       })
                     .eval();
  if ((min.array() >= max.array()).any()) {
    return;
  }
  for (auto z = min.z(); z != max.z(); ++z) {
    auto const k = z / Chunk::edge_length;
    auto const z_0 = static_cast<int>(k * Chunk::edge_length);
    for (auto y = min.y(); y != max.y(); ++y) {
      auto const j = y / Chunk::edge_length;
      auto const y_0 = static_cast<int>(j * Chunk::edge_length);
      for (auto x = min.x(); x != max.x(); ++x) {
        auto const i = x / Chunk::edge_length;
        auto const x_0 = static_cast<int>(i * Chunk::edge_length);
        auto const chunk_index =
          detail::linearize_chunk_offset(_chunk_counts, {i, j, k});
        auto &chunk = _chunks[chunk_index];
        chunk.set_solid({x - x_0, y - y_0, z - z_0}, solid);
      }
    }
  }
}

void Grid::set_solid(
  Eigen::Vector3i const &cell_indices, bool solid) noexcept {
  if (!bounds_check_cell(cell_indices)) {
    return;
  }
  auto const chunk_indices = (cell_indices / Chunk::edge_length).eval();
  auto const cell_offset = cell_indices - chunk_indices * Chunk::edge_length;
  get_chunk_unsafe(chunk_indices)->set_solid(cell_offset, solid);
}

bool Grid::is_solid(Eigen::Vector3i const &cell_indices) const noexcept {
  if (bounds_check_cell(cell_indices)) {
    auto const chunk_indices = (cell_indices / Chunk::edge_length).eval();
    auto const cell_offset = cell_indices - chunk_indices * Chunk::edge_length;
    return get_chunk_unsafe(chunk_indices)->is_solid(cell_offset);
  } else {
    return false;
  }
}

std::optional<Grid_raycast_hit> Grid::raycast(
  Eigen::Vector3i const &origin_cell_indices,
  Eigen::Vector3f const &origin_cell_offset,
  Eigen::Vector3f const &ray_direction,
  float max_t) const noexcept {
  assert(ray_direction.squaredNorm() > 0.0f);
  assert((origin_cell_offset.array() >= 0.0f).all());
  assert((origin_cell_offset.array() < 1.0f).all());
  if (max_t < 0.0f) {
    return std::nullopt;
  }
  auto cell_indices = origin_cell_indices;
  if (is_solid(cell_indices)) {
    return Grid_raycast_hit{
      .cell_indices = cell_indices,
      .normal = Eigen::Vector3i::Zero(),
      .t = 0.0f,
    };
  }
  auto step = Eigen::Vector3i{Eigen::Vector3i::Zero()};
  auto t_max = Eigen::Vector3f{Eigen::Vector3f::Zero()};
  auto t_delta = Eigen::Vector3f{Eigen::Vector3f::Zero()};
  for (auto axis = 0; axis != 3; ++axis) {
    auto const direction = ray_direction(axis);
    if (direction > 0.0f) {
      step(axis) = 1;
      t_max(axis) = (1.0f - origin_cell_offset(axis)) / direction;
      t_delta(axis) = 1.0f / direction;
    } else if (direction < 0.0f) {
      step(axis) = -1;
      t_max(axis) = origin_cell_offset(axis) / -direction;
      t_delta(axis) = 1.0f / -direction;
    } else {
      step(axis) = 0;
      t_max(axis) = std::numeric_limits<float>::infinity();
      t_delta(axis) = std::numeric_limits<float>::infinity();
    }
  }
  while (true) {
    auto const next_t = t_max.minCoeff();
    if (!(next_t <= max_t)) {
      return std::nullopt;
    }
    auto normal = Eigen::Vector3i{Eigen::Vector3i::Zero()};
    for (auto axis = 0; axis != 3; ++axis) {
      if (t_max(axis) == next_t) {
        cell_indices(axis) += step(axis);
        normal(axis) = -step(axis);
        t_max(axis) += t_delta(axis);
      }
    }
    if (is_solid(cell_indices)) {
      return Grid_raycast_hit{
        .cell_indices = cell_indices,
        .normal = normal,
        .t = next_t,
      };
    }
  }
}

Chunk *Grid::get_chunk(Eigen::Vector3i const &chunk_indices) noexcept {
  if (bounds_check_chunk(chunk_indices)) {
    return get_chunk_unsafe(chunk_indices);
  } else {
    return nullptr;
  }
}

Chunk const *
Grid::get_chunk(Eigen::Vector3i const &chunk_indices) const noexcept {
  if (bounds_check_chunk(chunk_indices)) {
    return get_chunk_unsafe(chunk_indices);
  } else {
    return nullptr;
  }
}

Chunk *Grid::get_chunk_unsafe(Eigen::Vector3i const &chunk_indices) noexcept {
  return &_chunks[detail::linearize_chunk_offset(
    _chunk_counts,
    {
      static_cast<std::size_t>(chunk_indices[0]),
      static_cast<std::size_t>(chunk_indices[1]),
      static_cast<std::size_t>(chunk_indices[2]),
    })];
}

Chunk const *
Grid::get_chunk_unsafe(Eigen::Vector3i const &chunk_indices) const noexcept {
  return &_chunks[detail::linearize_chunk_offset(
    _chunk_counts,
    {
      static_cast<std::size_t>(chunk_indices[0]),
      static_cast<std::size_t>(chunk_indices[1]),
      static_cast<std::size_t>(chunk_indices[2]),
    })];
}

bool Grid::bounds_check_cell(
  Eigen::Vector3i const &cell_indices) const noexcept {
  return cell_indices[0] >= 0 &&
         static_cast<std::size_t>(cell_indices[0]) < get_width() &&
         cell_indices[1] >= 0 &&
         static_cast<std::size_t>(cell_indices[1]) < get_height() &&
         cell_indices[2] >= 0 &&
         static_cast<std::size_t>(cell_indices[2]) < get_depth();
}

bool Grid::bounds_check_chunk(
  Eigen::Vector3i const &chunk_indices) const noexcept {
  return chunk_indices[0] >= 0 &&
         static_cast<std::size_t>(chunk_indices[0]) < _chunk_counts[0] &&
         chunk_indices[1] >= 0 &&
         static_cast<std::size_t>(chunk_indices[1]) < _chunk_counts[1] &&
         chunk_indices[2] >= 0 &&
         static_cast<std::size_t>(chunk_indices[2]) < _chunk_counts[2];
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
