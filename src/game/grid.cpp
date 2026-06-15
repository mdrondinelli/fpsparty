#include "grid.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

#include <serial/serialize.hpp>

namespace fpsparty::game {

bool Grid::diff(Grid const &lhs, Grid const &rhs) {
  if (
    lhs.get_cell_bounds().min() != rhs.get_cell_bounds().min() ||
    lhs.get_cell_bounds().max() != rhs.get_cell_bounds().max()) {
    return true;
  }
  return !std::ranges::equal(lhs._chunks, rhs._chunks);
}

Grid::Grid(Grid_create_info const &create_info)
    : _cell_bounds{create_info.bounds} {
  if (!empty()) {
    auto const chunk_counts = get_chunk_counts();
    auto const width_chunks = chunk_counts(0);
    auto const height_chunks = chunk_counts(1);
    auto const depth_chunks = chunk_counts(2);
    _chunks.resize(width_chunks * height_chunks * depth_chunks);
  }
}

void Grid::load(serial::Reader &reader) {
  using serial::deserialize;
  auto const cell_bounds = deserialize<math::ibox3>(reader);
  if (!cell_bounds) {
    throw Grid_loading_error{};
  }
  _cell_bounds = *cell_bounds;
  _chunks.clear();
  if (!empty()) {
    auto const chunk_bounds = cell_to_chunk(_cell_bounds);
    auto const chunk_counts =
      (chunk_bounds.diagonal() + math::ivec3::Ones()).eval();
    _chunks.resize(chunk_counts(0) * chunk_counts(1) * chunk_counts(2));
    for (auto &chunk : _chunks) {
      auto const blocks = deserialize<std::uint64_t>(reader);
      if (!blocks) {
        throw Grid_loading_error{};
      }
      chunk = Chunk{*blocks};
    }
  }
}

void Grid::dump(serial::Writer &writer) const {
  using serial::serialize;
  serialize<math::ibox3>(writer, get_cell_bounds());
  for (auto const &chunk : _chunks) {
    serialize<std::uint64_t>(writer, chunk.blocks);
  }
}

void Grid::fill(math::ibox3 const &cells, bool solid) {
  auto const bounded_cells = cells.intersection(get_cell_bounds());
  if (bounded_cells.isEmpty()) {
    return;
  }
  auto const &min = bounded_cells.min();
  auto const &max = bounded_cells.max();
  for (auto z = min.z(); z <= max.z(); ++z) {
    for (auto y = min.y(); y <= max.y(); ++y) {
      for (auto x = min.x(); x <= max.x(); ++x) {
        set_solid({x, y, z}, solid);
      }
    }
  }
}

std::optional<Grid_raycast_hit> Grid::raycast(
  Eigen::Vector3i const &origin_cell_coords,
  Eigen::Vector3f const &origin_cell_offset,
  Eigen::Vector3f const &ray_direction,
  float max_t) const noexcept {
  assert(ray_direction.squaredNorm() > 0.0f);
  assert((origin_cell_offset.array() >= 0.0f).all());
  assert((origin_cell_offset.array() < 1.0f).all());
  if (max_t < 0.0f) {
    return std::nullopt;
  }
  auto cell_coords = origin_cell_coords;
  if (is_solid(cell_coords)) {
    return Grid_raycast_hit{
      .cell_coords = cell_coords,
      .normal = math::ivec3::Zero(),
      .t = 0.0f,
    };
  }
  auto step = math::ivec3::Zero().eval();
  auto t_max = math::vec3::Zero().eval();
  auto t_delta = math::vec3::Zero().eval();
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
    auto normal = math::ivec3::Zero().eval();
    for (auto axis = 0; axis != 3; ++axis) {
      if (t_max(axis) == next_t) {
        cell_coords(axis) += step(axis);
        normal(axis) = -step(axis);
        t_max(axis) += t_delta(axis);
      }
    }
    if (is_solid(cell_coords)) {
      return Grid_raycast_hit{
        .cell_coords = cell_coords,
        .normal = normal,
        .t = next_t,
      };
    }
  }
}

void Grid::set_solid(math::ivec3 cell_coords, bool solid) noexcept {
  if (!bounds_check_cell(cell_coords)) {
    return;
  }
  auto const chunk_coords = cell_to_chunk(cell_coords);
  auto const cell_base = chunk_to_cell(chunk_coords);
  auto const cell_offset = (cell_coords - cell_base).eval();
  get_chunk_unsafe(chunk_coords)->set_solid(cell_offset, solid);
}

bool Grid::is_solid(math::ivec3 cell_coords) const noexcept {
  if (bounds_check_cell(cell_coords)) {
    auto const chunk_coords = cell_to_chunk(cell_coords);
    auto const cell_base = chunk_to_cell(chunk_coords);
    auto const cell_offset = (cell_coords - cell_base).eval();
    return get_chunk_unsafe(chunk_coords)->is_solid(cell_offset);
  } else {
    return false;
  }
}

bool Grid::bounds_check_cell(math::ivec3 coords) const noexcept {
  return get_cell_bounds().contains(coords);
}

Chunk *Grid::get_chunk_unsafe(math::ivec3 coords) noexcept {
  return const_cast<Chunk *>(std::as_const(*this).get_chunk_unsafe(coords));
}

Chunk const *Grid::get_chunk_unsafe(math::ivec3 coords) const noexcept {
  auto const indices = (coords - get_chunk_bounds().min()).eval();
  auto const index = linearize_chunk_index(indices);
  return &_chunks[index];
}

std::size_t Grid::linearize_chunk_index(math::ivec3 indices) const noexcept {
  return detail::linearize_chunk_index(get_chunk_counts(), indices);
}

Chunk_span Grid::get_chunks() noexcept {
  return {
    _chunks.data(),
    get_chunk_bounds(),
  };
}

Const_chunk_span Grid::get_chunks() const noexcept {
  return {
    _chunks.data(),
    get_chunk_bounds(),
  };
}

Const_chunk_span Grid::get_const_chunks() const noexcept {
  return get_chunks();
}

math::ivec3 Grid::get_chunk_counts() const noexcept {
  assert(!empty());
  return get_chunk_bounds().diagonal() + math::ivec3::Ones();
}

math::ivec3 Grid::get_cell_counts() const noexcept {
  assert(!empty());
  return get_cell_bounds().diagonal() + math::ivec3::Ones();
}

math::ibox3 Grid::get_chunk_bounds() const noexcept {
  return cell_to_chunk(get_cell_bounds());
}

math::ibox3 const &Grid::get_cell_bounds() const noexcept {
  return _cell_bounds;
}

bool Grid::empty() const noexcept {
  return get_cell_bounds().isEmpty();
}

math::ibox3 Grid::cell_to_chunk(math::ibox3 coords) {
  return math::ibox3{
    cell_to_chunk(coords.min()),
    cell_to_chunk(coords.max()),
  };
}

math::ivec3 Grid::cell_to_chunk(math::ivec3 coords) {
  return coords.unaryExpr(
    [](i32 coord) { return coord >> std::countr_zero(Chunk::edge_length); });
}

math::ivec3 Grid::chunk_to_cell(math::ivec3 coords) {
  return coords.unaryExpr(
    [](i32 coord) { return coord << std::countr_zero(Chunk::edge_length); });
}

} // namespace fpsparty::game
