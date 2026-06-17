#include "grid.hpp"

#include <algorithm>
#include <bit>
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
  math::ivec3 origin_cell_coords,
  math::vec3 origin_cell_offset,
  math::vec3 ray_direction,
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

std::optional<Grid_contact>
Grid::find_contact(math::box3 const &box) const noexcept {
  auto const touched_cells = Grid::world_to_cell(box);
  auto const &touched_min = touched_cells.min();
  auto const &touched_max = touched_cells.max();
  auto result = std::optional<Grid_contact>{};
  for (auto z = touched_min.z(); z <= touched_max.z(); ++z) {
    for (auto y = touched_min.y(); y <= touched_max.y(); ++y) {
      for (auto x = touched_min.x(); x <= touched_max.x(); ++x) {
        auto const cell_coords = math::ivec3{x, y, z};
        if (is_solid(cell_coords)) {
          auto const cell_box = math::box3{
            cell_coords.cast<f32>(),
            (cell_coords + math::ivec3::Ones()).cast<f32>(),
          };
          auto const pos_x_solid = is_solid({x + 1, y, z});
          auto const pos_y_solid = is_solid({x, y + 1, z});
          auto const pos_z_solid = is_solid({x, y, z + 1});
          auto const neg_x_solid = is_solid({x - 1, y, z});
          auto const neg_y_solid = is_solid({x, y - 1, z});
          auto const neg_z_solid = is_solid({x, y, z - 1});
          auto const pos = (cell_box.max() - box.min()).eval();
          auto const neg = (box.max() - cell_box.min()).eval();
          auto normal = math::ivec3::Zero().eval();
          auto penetration = std::numeric_limits<f32>::infinity();
          if (!pos_x_solid && pos.x() >= 0.0f && pos.x() < penetration) {
            normal = math::ivec3::UnitX();
            penetration = pos.x();
          }
          if (!pos_y_solid && pos.y() >= 0.0f && pos.y() < penetration) {
            normal = math::ivec3::UnitY();
            penetration = pos.y();
          }
          if (!pos_z_solid && pos.z() >= 0.0f && pos.z() < penetration) {
            normal = math::ivec3::UnitZ();
            penetration = pos.z();
          }
          if (!neg_x_solid && neg.x() >= 0.0f && neg.x() < penetration) {
            normal = -math::ivec3::UnitX();
            penetration = neg.x();
          }
          if (!neg_y_solid && neg.y() >= 0.0f && neg.y() < penetration) {
            normal = -math::ivec3::UnitY();
            penetration = neg.y();
          }
          if (!neg_z_solid && neg.z() >= 0.0f && neg.z() < penetration) {
            normal = -math::ivec3::UnitZ();
            penetration = neg.z();
          }
          if (!normal.isZero()) {
            auto const separation = -penetration;
            if (separation < 0.0f) {
              if (!result) {
                result = Grid_contact{
                  .cell_coords = cell_coords,
                  .normal = normal,
                  .separation = separation,
                };
              } else {
                auto const also_resolves_result_contact = [&] {
                  if (normal.x() < 0 && result->cell_coords.x() >= cell_coords.x()) {
                    return true;
                  }
                  if (normal.y() < 0 && result->cell_coords.y() >= cell_coords.y()) {
                    return true;
                  }
                  if (normal.z() < 0 && result->cell_coords.z() >= cell_coords.z()) {
                    return true;
                  }
                  if (normal.x() > 0 && result->cell_coords.x() <= cell_coords.x()) {
                    return true;
                  }
                  if (normal.y() > 0 && result->cell_coords.y() <= cell_coords.y()) {
                    return true;
                  }
                  if (normal.z() > 0 && result->cell_coords.z() <= cell_coords.z()) {
                    return true;
                  }
                  return false;
                  /*
                  // apply this contact to the untouched box to get the separated box
                  auto const separated_box = math::box3{
                    box.min() - separation * normal.cast<f32>(),
                    box.max() - separation * normal.cast<f32>(),
                  };
                  // find the bounds of the current result contact's originating cell
                  auto const result_cell_box = math::box3{
                    result->cell_coords.cast<f32>(),
                    (result->cell_coords + math::ivec3::Ones()).cast<f32>(),
                  };
                auto const intersection =
                  separated_box.intersection(result_cell_box);
                  return !intersection.isEmpty() && intersection.volume() > 0.0f;
                  */
                }();
                if (also_resolves_result_contact) {
                  // separated box does not intersect current result cell ->
                  // take least penetrated / most separated contact
                  if (separation > result->separation) {
                    result = Grid_contact{
                      .cell_coords = cell_coords,
                      .normal = normal,
                      .separation = separation,
                    };
                  }
                } else {
                  // separated box intersects current result cell ->
                  // take most penetrated / least separated contact
                  if (separation < result->separation) {
                    result = Grid_contact{
                      .cell_coords = cell_coords,
                      .normal = normal,
                      .separation = separation,
                    };
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return result;
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

bool Grid::empty() const noexcept { return get_cell_bounds().isEmpty(); }

math::ibox3 Grid::world_to_cell(math::box3 const &coords) {
  return math::ibox3{
    math::ivec3{coords.min().array().floor().cast<i32>()},
    math::ivec3{coords.max().array().ceil().cast<i32>()} - math::ivec3::Ones(),
  };
}

math::ibox3 Grid::cell_to_chunk(math::ibox3 const &coords) {
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
