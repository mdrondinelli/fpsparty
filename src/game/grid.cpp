#include "grid.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <limits>

#include <serial/serialize.hpp>

namespace fpsparty::game {

namespace {

struct Contact_constraint {
  math::ivec3 normal{};
  math::vec3 velocity{};
  float penetration{-std::numeric_limits<f32>::infinity()};
  float surface_area{0.0f};
};

void update_contact_constraint(
  Contact_constraint &constraint,
  math::ivec3 normal,
  math::vec3 velocity,
  float penetration,
  float surface_area) noexcept {
  if (penetration > constraint.penetration) {
    constraint = Contact_constraint{
      .normal = normal,
      .velocity = velocity,
      .penetration = penetration,
      .surface_area = surface_area,
    };
  } else if (penetration == constraint.penetration) {
    constraint.velocity = (constraint.velocity * constraint.surface_area +
                           velocity * surface_area) /
                          (constraint.surface_area + surface_area);
  }
}

} // namespace

bool Grid::diff(Grid const &lhs, Grid const &rhs) {
  if (
    lhs.get_cell_bounds().min() != rhs.get_cell_bounds().min() ||
    lhs.get_cell_bounds().max() != rhs.get_cell_bounds().max()) {
    return true;
  }
  return !std::ranges::equal(lhs._chunks, rhs._chunks);
}

Grid::Grid(Grid_create_info const &create_info)
    : _block_bounds_registry{create_info.block_bounds_registry},
      _bounds{create_info.bounds} {
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
  _bounds = *cell_bounds;
  _chunks.clear();
  if (!empty()) {
    auto const chunk_bounds = cell_to_chunk(_bounds);
    auto const chunk_counts =
      (chunk_bounds.diagonal() + math::ivec3::Ones()).eval();
    _chunks.resize(chunk_counts(0) * chunk_counts(1) * chunk_counts(2));
    for (auto &chunk : _chunks) {
      chunk = Chunk{};
      for (auto &byte : chunk.bytes) {
        auto const deserialized_byte = deserialize<std::byte>(reader);
        if (!deserialized_byte) {
          throw Grid_loading_error{};
        }
        byte = *deserialized_byte;
      }
    }
  }
}

void Grid::dump(serial::Writer &writer) const {
  using serial::serialize;
  serialize<math::ibox3>(writer, get_cell_bounds());
  for (auto const &chunk : _chunks) {
    for (auto const &byte : chunk.bytes) {
      serialize<std::byte>(writer, byte);
    }
  }
}

void Grid::fill(math::ibox3 const &cells, Block block, int data) {
  auto const bounded_cells = cells.intersection(get_cell_bounds());
  if (bounded_cells.isEmpty()) {
    return;
  }
  auto const &min = bounded_cells.min();
  auto const &max = bounded_cells.max();
  for (auto z = min.z(); z <= max.z(); ++z) {
    for (auto y = min.y(); y <= max.y(); ++y) {
      for (auto x = min.x(); x <= max.x(); ++x) {
        set_block({x, y, z}, block, data);
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
  auto const box_xy_bounds = math::box2{
    math::vec2{box.min().x(), box.min().y()},
    math::vec2{box.max().x(), box.max().y()},
  };
  auto const box_xz_bounds = math::box2{
    math::vec2{box.min().x(), box.min().z()},
    math::vec2{box.max().x(), box.max().z()},
  };
  auto const box_yz_bounds = math::box2{
    math::vec2{box.min().y(), box.min().z()},
    math::vec2{box.max().y(), box.max().z()},
  };
  auto pos_x_constraint = Contact_constraint{};
  auto pos_y_constraint = Contact_constraint{};
  auto pos_z_constraint = Contact_constraint{};
  auto neg_x_constraint = Contact_constraint{};
  auto neg_y_constraint = Contact_constraint{};
  auto neg_z_constraint = Contact_constraint{};
  for (auto z = touched_min.z(); z <= touched_max.z(); ++z) {
    for (auto y = touched_min.y(); y <= touched_max.y(); ++y) {
      for (auto x = touched_min.x(); x <= touched_max.x(); ++x) {
        auto const cell_coords = math::ivec3{x, y, z};
        auto const [cell_block, cell_data] = get_block(cell_coords);
        if (cell_block != Block::air) {
          auto const block_bounds = _block_bounds_registry->get(cell_block);
          auto const cell_bounds = math::box3{
            cell_coords.cast<f32>() + block_bounds.min(),
            cell_coords.cast<f32>() + block_bounds.max(),
          };
          auto const pos = (cell_bounds.max() - box.min()).eval();
          auto const neg = (box.max() - cell_bounds.min()).eval();
          if (
            pos.x() <= 0.0f || pos.y() <= 0.0f || pos.z() <= 0.0f ||
            neg.x() <= 0.0f || neg.y() <= 0.0f || neg.z() <= 0.0f) {
            continue;
          }
          auto const cell_xy_bounds = math::box2{
            math::vec2{cell_bounds.min().x(), cell_bounds.min().y()},
            math::vec2{cell_bounds.max().x(), cell_bounds.max().y()},
          };
          auto const cell_xz_bounds = math::box2{
            math::vec2{cell_bounds.min().x(), cell_bounds.min().z()},
            math::vec2{cell_bounds.max().x(), cell_bounds.max().z()},
          };
          auto const cell_yz_bounds = math::box2{
            math::vec2{cell_bounds.min().y(), cell_bounds.min().z()},
            math::vec2{cell_bounds.max().y(), cell_bounds.max().z()},
          };
          auto normal = math::ivec3::Zero().eval();
          auto penetration = std::numeric_limits<f32>::infinity();
          auto surface_area = 0.0f;
          if (pos.x() < penetration) {
            normal = math::ivec3::UnitX();
            penetration = pos.x();
            surface_area = cell_yz_bounds.intersection(box_yz_bounds).volume();
          }
          if (pos.y() < penetration) {
            normal = math::ivec3::UnitY();
            penetration = pos.y();
            surface_area = cell_xz_bounds.intersection(box_xz_bounds).volume();
          }
          if (pos.z() < penetration) {
            normal = math::ivec3::UnitZ();
            penetration = pos.z();
            surface_area = cell_xy_bounds.intersection(box_xy_bounds).volume();
          }
          if (neg.x() < penetration) {
            normal = -math::ivec3::UnitX();
            penetration = neg.x();
            surface_area = cell_yz_bounds.intersection(box_yz_bounds).volume();
          }
          if (neg.y() < penetration) {
            normal = -math::ivec3::UnitY();
            penetration = neg.y();
            surface_area = cell_xz_bounds.intersection(box_xz_bounds).volume();
          }
          if (neg.z() < penetration) {
            normal = -math::ivec3::UnitZ();
            penetration = neg.z();
            surface_area = cell_xy_bounds.intersection(box_xy_bounds).volume();
          }
          if (normal == math::ivec3::UnitX()) {
            update_contact_constraint(
              pos_x_constraint,
              normal,
              math::vec3::Zero(),
              penetration,
              surface_area);
          } else if (normal == math::ivec3::UnitY()) {
            if (cell_block == Block::conveyor) {
              auto const velocity =
                (9.0f / 16.0f * [&] -> math::vec3 {
                  auto const p =
                    (math::vec2{box.center().x(), box.center().z()} -
                     math::vec2{cell_bounds.min().x(), cell_bounds.min().z()})
                      .cwiseMax(math::vec2::Zero())
                      .cwiseMin(math::vec2::Ones())
                      .eval();
                  auto const [pos_x_block, pos_x_data] =
                    get_block({x + 1, y, z});
                  auto const [neg_x_block, neg_x_data] =
                    get_block({x - 1, y, z});
                  auto const [pos_z_block, pos_z_data] =
                    get_block({x, y, z + 1});
                  auto const [neg_z_block, neg_z_data] =
                    get_block({x, y, z - 1});
                  auto const pos_x_neighbor =
                    pos_x_block == Block::conveyor && pos_x_data == 0b11;
                  auto const neg_x_neighbor =
                    neg_x_block == Block::conveyor && neg_x_data == 0b01;
                  auto const pos_z_neighbor =
                    pos_z_block == Block::conveyor && pos_z_data == 0b10;
                  auto const neg_z_neighbor =
                    neg_z_block == Block::conveyor && neg_z_data == 0b00;
                  if (cell_data == 0b00) {
                    // north / +z
                    if (neg_x_neighbor && p.x() < 0.25f) {
                      auto const t = p.x() / 0.25f;
                      return t * math::vec3::UnitZ() +
                             (1.0f - t) * math::vec3::UnitX();
                    } else if (pos_x_neighbor && p.x() > 0.75f) {
                      auto const t = 1.0f - (p.x() - 0.75f) / 0.25f;
                      return t * math::vec3::UnitZ() +
                             (1.0f - t) * -math::vec3::UnitX();
                    } else {
                      return {0.0f, 0.0f, 1.0f};
                    }
                  } else if (cell_data == 0b01) {
                    // west / +x
                    if (neg_z_neighbor && p.y() < 0.25f) {
                      auto const t = p.y() / 0.25f;
                      return t * math::vec3::UnitX() +
                             (1.0f - t) * math::vec3::UnitZ();
                    } else if (pos_z_neighbor && p.y() > 0.75f) {
                      auto const t = 1.0f - (p.y() - 0.75f) / 0.25f;
                      return t * math::vec3::UnitX() +
                             (1.0f - t) * -math::vec3::UnitZ();
                    } else {
                      return {1.0f, 0.0f, 0.0f};
                    }
                  } else if (cell_data == 0b10) {
                    // south / -z
                    if (neg_x_neighbor && p.x() < 0.25f) {
                      auto const t = p.x() / 0.25f;
                      return t * -math::vec3::UnitZ() +
                             (1.0f - t) * math::vec3::UnitX();
                    } else if (pos_x_neighbor && p.x() > 0.75f) {
                      auto const t = 1.0f - (p.x() - 0.75f) / 0.25f;
                      return t * -math::vec3::UnitZ() +
                             (1.0f - t) * -math::vec3::UnitX();
                    } else {
                      return {0.0f, 0.0f, -1.0f};
                    }
                  } else /*if (cell_data == 0b11)*/ {
                    // east / -x
                    if (neg_z_neighbor && p.y() < 0.25f) {
                      auto const t = p.y() / 0.25f;
                      return t * -math::vec3::UnitX() +
                             (1.0f - t) * math::vec3::UnitZ();
                    } else if (pos_z_neighbor && p.y() > 0.75f) {
                      auto const t = 1.0f - (p.y() - 0.75f) / 0.25f;
                      return t * -math::vec3::UnitX() +
                             (1.0f - t) * -math::vec3::UnitZ();
                    } else {
                      return {-1.0f, 0.0f, 0.0f};
                    }
                  }
                }())
                  .eval();
              update_contact_constraint(
                pos_y_constraint, normal, velocity, penetration, surface_area);
            } else {
              update_contact_constraint(
                pos_y_constraint,
                normal,
                math::vec3::Zero(),
                penetration,
                surface_area);
            }
          } else if (normal == math::ivec3::UnitZ()) {
            update_contact_constraint(
              pos_z_constraint,
              normal,
              math::vec3::Zero(),
              penetration,
              surface_area);
          } else if (normal == -math::ivec3::UnitX()) {
            update_contact_constraint(
              neg_x_constraint,
              normal,
              math::vec3::Zero(),
              penetration,
              surface_area);
          } else if (normal == -math::ivec3::UnitY()) {
            update_contact_constraint(
              neg_y_constraint,
              normal,
              math::vec3::Zero(),
              penetration,
              surface_area);
          } else if (normal == -math::ivec3::UnitZ()) {
            update_contact_constraint(
              neg_z_constraint,
              normal,
              math::vec3::Zero(),
              penetration,
              surface_area);
          }
        }
      }
    }
  }
  auto result = std::optional<Grid_contact>{};
  auto const update_result = [&](Contact_constraint const &constraint) {
    if (constraint.penetration < 0.0f) {
      return;
    }
    auto const separation = -constraint.penetration;
    if (!result || separation < result->separation) {
      result = Grid_contact{
        .normal = constraint.normal,
        .velocity = constraint.velocity,
        .separation = separation,
      };
    }
  };
  update_result(pos_x_constraint);
  update_result(pos_y_constraint);
  update_result(pos_z_constraint);
  update_result(neg_x_constraint);
  update_result(neg_y_constraint);
  update_result(neg_z_constraint);
  return result;
}

bool Grid::set_block(math::ivec3 cell_coords, Block block, int data) noexcept {
  if (!bounds_check_cell(cell_coords)) {
    return false;
  }
  auto const chunk_coords = cell_to_chunk(cell_coords);
  auto const cell_base = chunk_to_cell(chunk_coords);
  auto const cell_offset = (cell_coords - cell_base).eval();
  get_chunk_unsafe(chunk_coords)->set_block(cell_offset, block, data);
  return true;
}

std::pair<Block, int> Grid::get_block(math::ivec3 cell_coords) const noexcept {
  if (!bounds_check_cell(cell_coords)) {
    return {Block::air, 0};
  }
  auto const chunk_coords = cell_to_chunk(cell_coords);
  auto const cell_base = chunk_to_cell(chunk_coords);
  auto const cell_offset = (cell_coords - cell_base).eval();
  return get_chunk_unsafe(chunk_coords)->get_block(cell_offset);
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

math::ibox3 const &Grid::get_cell_bounds() const noexcept { return _bounds; }

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
