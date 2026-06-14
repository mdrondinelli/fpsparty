#include "grid.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

#include <serial/serialize.hpp>

namespace fpsparty::game {

bool Grid::diff(Grid const &lhs, Grid const &rhs) {
  if (
    lhs.get_width() != rhs.get_width() ||
    lhs.get_height() != rhs.get_height() ||
    lhs.get_depth() != rhs.get_depth()) {
    return true;
  }
  return !std::ranges::equal(lhs._chunks, rhs._chunks);
}

Grid::Grid(Grid_create_info const &create_info) : _bounds{create_info.bounds} {
  auto const chunk_counts = get_chunk_counts();
  auto const width_chunks = chunk_counts(0);
  auto const height_chunks = chunk_counts(1);
  auto const depth_chunks = chunk_counts(2);
  _chunks.resize(width_chunks * height_chunks * depth_chunks);
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

void Grid::set_solid(Eigen::Vector3i const &cell_coords, bool solid) noexcept {
  if (!bounds_check_cell(cell_coords)) {
    return;
  }
  auto const chunk_coords =
    (cell_coords & ~static_cast<int>(Chunk::edge_length - 1)).eval();
  auto const cell_offset =
    (cell_coords - chunk_coords * Chunk::edge_length).eval();
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

bool Grid::bounds_check_cell(
  Eigen::Vector3i const &cell_coords) const noexcept {
  return _bounds.contains(cell_coords);
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

math::ivec3 Grid::get_chunk_counts() const noexcept {
  auto const cell_counts = get_cell_counts();
  assert(cell_counts(0) % Chunk::edge_length == 0);
  assert(cell_counts(1) % Chunk::edge_length == 0);
  assert(cell_counts(2) % Chunk::edge_length == 0);
  return cell_counts / Chunk::edge_length;
}

math::ivec3 Grid::get_cell_counts() const noexcept {
  return _bounds.diagonal() + math::ivec3::Ones();
}

math::ibox3 const &Grid::get_bounds() const noexcept { return _bounds; }

} // namespace fpsparty::game
