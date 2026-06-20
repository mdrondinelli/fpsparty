#include "grid_mesh.hpp"
#include "client/grid_vertex.hpp"
#include "game/constants.hpp"
#include "game/grid.hpp"
#include "serial/span_writer.hpp"

#include <array>
#include <cassert>
#include <span>
#include <vector>

namespace fpsparty::client {
namespace {
struct Chunk_geometry {
  std::array<std::array<std::vector<Grid_vertex>, 2>, 3> vertices;
  std::array<std::array<std::vector<std::uint32_t>, 2>, 3> indices;
};

constexpr int sign_index(Sign sign) noexcept { return static_cast<int>(sign); }

void append_face(
  Chunk_geometry &result,
  game::Axis axis,
  Sign sign,
  std::array<math::vec3, 4> const &positions,
  std::uint32_t texture_index) {
  auto const axis_index = static_cast<int>(axis);
  auto const side_index = sign_index(sign);
  auto &vertices = result.vertices[axis_index][side_index];
  auto &indices = result.indices[axis_index][side_index];
  auto const first_index = static_cast<std::uint32_t>(vertices.size());
  for (auto const &position : positions) {
    vertices.push_back({
        .position = position,
        .texture_index = texture_index,
    });
  }
  indices.append_range(
    std::array<std::uint32_t, 6>{
      first_index + 0,
      first_index + 1,
      first_index + 2,
      first_index + 2,
      first_index + 3,
      first_index + 0,
    });
}

void append_solid_cell_faces(
  Chunk_geometry &result,
  game::Grid const &grid,
  math::ivec3 cell_coords) {
  auto const min =
    (cell_coords.cast<float>() * game::constants::grid_cell_stride).eval();
  auto const max =
    (min + math::vec3::Constant(game::constants::grid_cell_stride)).eval();
  auto const x0 = min.x();
  auto const y0 = min.y();
  auto const z0 = min.z();
  auto const x1 = max.x();
  auto const y1 = max.y();
  auto const z1 = max.z();
  auto const texture_index = 
      static_cast<std::uint32_t>(grid.get_block(cell_coords).first) - 1;
  if (!grid.is_solid(cell_coords + math::ivec3{1, 0, 0})) {
    append_face(
      result,
      game::Axis::x,
      Sign::positive,
      {
        math::vec3{x1, y0, z0},
        math::vec3{x1, y1, z0},
        math::vec3{x1, y1, z1},
        math::vec3{x1, y0, z1},
      },
      texture_index);
  }
  if (!grid.is_solid(cell_coords - math::ivec3{1, 0, 0})) {
    append_face(
      result,
      game::Axis::x,
      Sign::negative,
      {
        math::vec3{x0, y0, z0},
        math::vec3{x0, y0, z1},
        math::vec3{x0, y1, z1},
        math::vec3{x0, y1, z0},
      },
      texture_index);
  }
  if (!grid.is_solid(cell_coords + math::ivec3{0, 1, 0})) {
    append_face(
      result,
      game::Axis::y,
      Sign::positive,
      {
        math::vec3{x0, y1, z0},
        math::vec3{x0, y1, z1},
        math::vec3{x1, y1, z1},
        math::vec3{x1, y1, z0},
      },
      texture_index);
  }
  if (!grid.is_solid(cell_coords - math::ivec3{0, 1, 0})) {
    append_face(
      result,
      game::Axis::y,
      Sign::negative,
      {
        math::vec3{x0, y0, z0},
        math::vec3{x1, y0, z0},
        math::vec3{x1, y0, z1},
        math::vec3{x0, y0, z1},
      },
      texture_index);
  }
  if (!grid.is_solid(cell_coords + math::ivec3{0, 0, 1})) {
    append_face(
      result,
      game::Axis::z,
      Sign::positive,
      {
        math::vec3{x0, y0, z1},
        math::vec3{x1, y0, z1},
        math::vec3{x1, y1, z1},
        math::vec3{x0, y1, z1},
      },
      texture_index);
  }
  if (!grid.is_solid(cell_coords - math::ivec3{0, 0, 1})) {
    append_face(
      result,
      game::Axis::z,
      Sign::negative,
      {
        math::vec3{x0, y0, z0},
        math::vec3{x0, y1, z0},
        math::vec3{x1, y1, z0},
        math::vec3{x1, y0, z0},
      },
      texture_index);
  }
}

Chunk_geometry generate_chunk_geometry(
  math::ivec3 chunk_coords,
  game::Chunk const &chunk,
  game::Grid const &grid) {
  auto result = Chunk_geometry{};
  constexpr auto n = static_cast<int>(game::Chunk::edge_length);
  for (auto x = 0; x != n; ++x) {
    for (auto y = 0; y != n; ++y) {
      for (auto z = 0; z != n; ++z) {
        if (!chunk.is_solid({x, y, z})) {
          continue;
        }
        append_solid_cell_faces(
          result,
          grid,
          {
            chunk_coords.x() * n + x,
            chunk_coords.y() * n + y,
            chunk_coords.z() * n + z,
          });
      }
    }
  }
  return result;
}
} // namespace

Grid_mesh::Grid_mesh(Grid_mesh_create_info const &info) {
  auto vertices = std::vector<Grid_vertex>{};
  auto indices = std::vector<std::uint32_t>{};
  auto draw_infos =
    std::array<std::array<std::vector<graphics::Indexed_draw_info>, 2>, 3>{};
  for (auto const &[chunk_coords, chunk] : info.grid->get_chunks()) {
    auto const geometry =
      generate_chunk_geometry(chunk_coords, *chunk, *info.grid);
    for (auto axis = 0; axis != 3; ++axis) {
      for (auto sign = 0; sign != 2; ++sign) {
        auto const draw_info = graphics::Indexed_draw_info{
          .index_count =
            static_cast<std::uint32_t>(geometry.indices[axis][sign].size()),
          .instance_count = 1,
          .first_index = static_cast<std::uint32_t>(indices.size()),
          .vertex_offset = static_cast<std::int32_t>(vertices.size()),
          .first_instance = 0,
        };
        if (draw_info.index_count) {
          vertices.append_range(geometry.vertices[axis][sign]);
          indices.append_range(geometry.indices[axis][sign]);
          draw_infos[axis][sign].emplace_back(draw_info);
        }
      }
    }
  }
  auto draw_count = std::uint64_t{};
  for (auto axis = 0; axis != 3; ++axis) {
    for (auto sign = 0; sign != 2; ++sign) {
      draw_count += draw_infos[axis][sign].size();
    }
  }
  auto const vertex_buffer_size = vertices.size() * sizeof(Grid_vertex);
  auto const index_buffer_size = indices.size() * sizeof(std::uint32_t);
  auto const draw_buffer_size =
    draw_count * sizeof(graphics::Indexed_draw_info);
  if (vertex_buffer_size > 0 && index_buffer_size > 0 && draw_buffer_size > 0) {
    auto const staging_buffer = info.graphics->create_staging_buffer(
      vertex_buffer_size + index_buffer_size + draw_buffer_size);
    auto const staging_buffer_memory = staging_buffer->map();
    auto staging_buffer_writer =
      serial::Span_writer{staging_buffer_memory.get()};
    auto const vertex_buffer_offset = staging_buffer_writer.offset();
    staging_buffer_writer.write(std::as_bytes(std::span{vertices}));
    auto const index_buffer_offset = staging_buffer_writer.offset();
    staging_buffer_writer.write(std::as_bytes(std::span{indices}));
    auto const draw_buffer_offset = staging_buffer_writer.offset();
    for (auto axis = 0; axis != 3; ++axis) {
      for (auto sign = 0; sign != 2; ++sign) {
        _draw_infos[axis][sign].offset =
          staging_buffer_writer.offset() - draw_buffer_offset;
        _draw_infos[axis][sign].draw_count = draw_infos[axis][sign].size();
        staging_buffer_writer
          .write(std::as_bytes(std::span{draw_infos[axis][sign]}));
      }
    }
    _vertex_buffer = info.graphics->create_buffer({
      .size = vertex_buffer_size,
      .usage = graphics::Buffer_usage_flag_bits::transfer_dst |
               graphics::Buffer_usage_flag_bits::shader_device_address,
    });
    _index_buffer = info.graphics->create_index_buffer(index_buffer_size);
    _draw_buffer = info.graphics->create_buffer({
      .size = draw_buffer_size,
      .usage = graphics::Buffer_usage_flag_bits::transfer_dst |
               graphics::Buffer_usage_flag_bits::indirect_buffer,
    });
    auto recorder = info.graphics->record_transient_work();
    recorder.copy_buffer(
      staging_buffer,
      _vertex_buffer,
      {
        .src_offset = vertex_buffer_offset,
        .dst_offset = 0,
        .size = vertex_buffer_size,
      });
    recorder.copy_buffer(
      staging_buffer,
      _index_buffer,
      {
        .src_offset = index_buffer_offset,
        .dst_offset = 0,
        .size = index_buffer_size,
      });
    recorder.copy_buffer(
      staging_buffer,
      _draw_buffer,
      {
        .src_offset = draw_buffer_offset,
        .dst_offset = 0,
        .size = draw_buffer_size,
      });
    recorder.barrier(
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
        .access_mask = graphics::Access_flag_bits::transfer_write,
      },
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::draw_indirect |
                      graphics::Pipeline_stage_flag_bits::index_input |
                      graphics::Pipeline_stage_flag_bits::vertex_shader,
        .access_mask = graphics::Access_flag_bits::indirect_command_read |
                       graphics::Access_flag_bits::index_read |
                       graphics::Access_flag_bits::shader_storage_read,
      });
    _upload_work = info.graphics->submit_transient_work(std::move(recorder));
    _upload_work->add_done_callback(this);
  }
}

Grid_mesh::~Grid_mesh() {
  if (_upload_work) {
    _upload_work->remove_done_callback(this);
  }
}

void Grid_mesh::record_draws(
  graphics::Work_recorder &recorder, game::Axis axis, client::Sign sign) {
  assert(is_uploaded());
  auto const draw_info =
    _draw_infos[static_cast<int>(axis)][static_cast<int>(sign)];
  recorder.draw_indexed_indirect({
    .buffer = _draw_buffer,
    .offset = draw_info.offset,
    .draw_count = draw_info.draw_count,
    .stride = sizeof(graphics::Indexed_draw_info),
  });
}

bool Grid_mesh::is_uploaded() const noexcept {
  return _vertex_buffer && _index_buffer && !_upload_work;
}

rc::Strong<graphics::Buffer> const &
Grid_mesh::get_vertex_buffer() const noexcept {
  return _vertex_buffer;
}

rc::Strong<graphics::Buffer> const &
Grid_mesh::get_index_buffer() const noexcept {
  return _index_buffer;
}

void Grid_mesh::on_work_done(graphics::Work const &) { _upload_work = nullptr; }
} // namespace fpsparty::client
