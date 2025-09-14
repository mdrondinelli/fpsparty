#include "grid_mesh.hpp"
#include "client/grid_vertex.hpp"
#include "game/core/constants.hpp"
#include "game/core/grid.hpp"
#include "serial/span_writer.hpp"
#include <vector>

namespace fpsparty::client {
namespace {
std::pair<std::array<Grid_vertex, 4>, std::array<std::uint32_t, 6>>
generate_face_geometry(
  const Eigen::Vector3i &cell_indices,
  const Eigen::Vector3f &u,
  const Eigen::Vector3f &v,
  std::uint32_t face_begin_index) {
  const auto cell_position =
    (cell_indices.cast<float>() * game::constants::grid_cell_stride).eval();
  const auto vertices = std::array<Grid_vertex, 4>{
    Grid_vertex{
      .position = cell_position +
                  u * game::constants::grid_wall_thickness * 0.5f +
                  v * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position = cell_position +
                  u * (game::constants::grid_cell_stride -
                       game::constants::grid_wall_thickness * 0.5f) +
                  v * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position = cell_position +
                  u * (game::constants::grid_cell_stride -
                       game::constants::grid_wall_thickness * 0.5f) +
                  v * (game::constants::grid_cell_stride -
                       game::constants::grid_wall_thickness * 0.5f),
    },
    Grid_vertex{
      .position = cell_position +
                  u * game::constants::grid_wall_thickness * 0.5f +
                  v * (game::constants::grid_cell_stride -
                       game::constants::grid_wall_thickness * 0.5f),
    },
  };
  const auto indices = std::array<std::uint32_t, 6>{
    face_begin_index + 0,
    face_begin_index + 1,
    face_begin_index + 2,
    face_begin_index + 2,
    face_begin_index + 3,
    face_begin_index + 0,
  };
  return {vertices, indices};
}

struct Faces_geometry_generation_result {
  std::array<std::vector<Grid_vertex>, 3> vertices;
  std::array<std::vector<std::uint32_t>, 3> indices;
};

Faces_geometry_generation_result
generate_faces_geometry(const game::Grid &grid) {
  auto retval = Faces_geometry_generation_result{};
  auto &x_vertices = retval.vertices[0];
  auto &y_vertices = retval.vertices[1];
  auto &z_vertices = retval.vertices[2];
  auto &x_indices = retval.indices[0];
  auto &y_indices = retval.indices[1];
  auto &z_indices = retval.indices[2];
  constexpr auto n = static_cast<int>(game::Chunk::edge_length);
  for (const auto &[chunk_indices, chunk] : grid.get_chunks()) {
    for (auto x = 0; x != n; ++x) {
      for (auto y = 0; y != n; ++y) {
        for (auto z = 0; z != n; ++z) {
          const auto cell_indices = Eigen::Vector3i{
            chunk_indices.x() * n + x,
            chunk_indices.y() * n + y,
            chunk_indices.z() * n + z,
          };
          if (chunk->is_solid(game::Axis::x, {x, y, z})) {
            const auto [face_vertices, face_indices] = generate_face_geometry(
              cell_indices,
              {0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f, 1.0f},
              static_cast<std::uint32_t>(x_vertices.size()));
            x_vertices.append_range(face_vertices);
            x_indices.append_range(face_indices);
          }
          if (chunk->is_solid(game::Axis::y, {x, y, z})) {
            const auto [face_vertices, face_indices] = generate_face_geometry(
              cell_indices,
              {0.0f, 0.0f, 1.0f},
              {1.0f, 0.0f, 0.0f},
              static_cast<std::uint32_t>(y_vertices.size()));
            y_vertices.append_range(face_vertices);
            y_indices.append_range(face_indices);
          }
          if (chunk->is_solid(game::Axis::z, {x, y, z})) {
            const auto [face_vertices, face_indices] = generate_face_geometry(
              cell_indices,
              {1.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 0.0f},
              static_cast<std::uint32_t>(z_vertices.size()));
            z_vertices.append_range(face_vertices);
            z_indices.append_range(face_indices);
          }
        }
      }
    }
  }
  return retval;
}
} // namespace

Grid_mesh::Grid_mesh(const Grid_mesh_create_info &info) {
  const auto faces_geometry = generate_faces_geometry(*info.grid);
  auto &x_face_vertices = faces_geometry.vertices[0];
  auto &y_face_vertices = faces_geometry.vertices[1];
  auto &z_face_vertices = faces_geometry.vertices[2];
  auto &x_face_indices = faces_geometry.indices[0];
  auto &y_face_indices = faces_geometry.indices[1];
  auto &z_face_indices = faces_geometry.indices[2];
  const auto vertex_buffer_size =
    (x_face_vertices.size() + y_face_vertices.size() + z_face_vertices.size()) *
    sizeof(Grid_vertex);
  const auto index_buffer_size =
    (x_face_indices.size() + y_face_indices.size() + z_face_indices.size()) *
    sizeof(std::uint32_t);
  if (vertex_buffer_size > 0 && index_buffer_size > 0) {
    auto staging_data = std::vector<std::byte>{};
    staging_data.resize(vertex_buffer_size + index_buffer_size);
    auto staging_data_writer = serial::Span_writer{staging_data};
    staging_data_writer.write(std::as_bytes(std::span{x_face_vertices}));
    staging_data_writer.write(std::as_bytes(std::span{y_face_vertices}));
    staging_data_writer.write(std::as_bytes(std::span{z_face_vertices}));
    staging_data_writer.write(std::as_bytes(std::span{x_face_indices}));
    staging_data_writer.write(std::as_bytes(std::span{y_face_indices}));
    staging_data_writer.write(std::as_bytes(std::span{z_face_indices}));
    const auto staging_buffer =
      info.graphics->create_staging_buffer(staging_data);
    _vertex_buffer = info.graphics->create_vertex_buffer(vertex_buffer_size);
    _index_buffer = info.graphics->create_index_buffer(index_buffer_size);
    auto recorder = info.graphics->record_transient_work();
    recorder.copy_buffer(
      staging_buffer,
      _vertex_buffer,
      {
        .src_offset = 0,
        .dst_offset = 0,
        .size = vertex_buffer_size,
      });
    recorder.copy_buffer(
      staging_buffer,
      _index_buffer,
      {
        .src_offset = vertex_buffer_size,
        .dst_offset = 0,
        .size = index_buffer_size,
      });
    _upload_work = info.graphics->submit_transient_work(std::move(recorder));
    _upload_work->add_done_callback(this);
    _face_draw_infos[0].index_count = x_face_indices.size();
    _face_draw_infos[1].index_count = y_face_indices.size();
    _face_draw_infos[2].index_count = z_face_indices.size();
    _face_draw_infos[1].first_index = x_face_indices.size();
    _face_draw_infos[2].first_index =
      x_face_indices.size() + y_face_indices.size();
    _face_draw_infos[1].vertex_offset = x_face_vertices.size();
    _face_draw_infos[2].vertex_offset =
      x_face_vertices.size() + y_face_vertices.size();
  }
}

Grid_mesh::~Grid_mesh() {
  if (_upload_work) {
    _upload_work->remove_done_callback(this);
  }
}

void Grid_mesh::record_face_drawing_command(
  graphics::Work_recorder &recorder, game::Axis axis) {
  assert(is_uploaded());
  recorder.draw_indexed(_face_draw_infos[static_cast<int>(axis)]);
}

bool Grid_mesh::is_uploaded() const noexcept {
  return _vertex_buffer && _index_buffer && !_upload_work;
}

const rc::Strong<graphics::Vertex_buffer> &
Grid_mesh::get_vertex_buffer() const noexcept {
  return _vertex_buffer;
}

const rc::Strong<graphics::Index_buffer> &
Grid_mesh::get_index_buffer() const noexcept {
  return _index_buffer;
}

void Grid_mesh::on_work_done(const graphics::Work &) { _upload_work = nullptr; }
} // namespace fpsparty::client
