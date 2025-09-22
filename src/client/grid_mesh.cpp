#include "grid_mesh.hpp"
#include "client/grid_vertex.hpp"
#include "game/core/constants.hpp"
#include "game/core/grid.hpp"
#include "serial/span_writer.hpp"
#include <Eigen/src/Core/Matrix.h>
#include <algorithm>
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
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position + u * game::constants::grid_wall_thickness * 0.5f +
        v * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position +
        u * (game::constants::grid_cell_stride -
             game::constants::grid_wall_thickness * 0.5f) +
        v * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position +
        u * (game::constants::grid_cell_stride -
             game::constants::grid_wall_thickness * 0.5f) +
        v * (game::constants::grid_cell_stride -
             game::constants::grid_wall_thickness * 0.5f),
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position + u * game::constants::grid_wall_thickness * 0.5f +
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

std::pair<std::array<Grid_vertex, 4>, std::array<std::uint32_t, 6>>
generate_edge_geometry(
  const Eigen::Vector3i &cell_indices,
  const Eigen::Vector3f &major_axis,
  const Eigen::Vector3f &minor_axis,
  const Eigen::Vector3f &normal,
  std::uint32_t face_begin_index) {
  const auto cell_position =
    (cell_indices.cast<float>() * game::constants::grid_cell_stride).eval();
  const auto vertices = std::array<Grid_vertex, 4>{
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position +
        major_axis * game::constants::grid_wall_thickness * 0.5f +
        minor_axis * game::constants::grid_wall_thickness * 0.5f +
        normal * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position +
        major_axis * (game::constants::grid_cell_stride -
                      game::constants::grid_wall_thickness * 0.5f) +
        minor_axis * game::constants::grid_wall_thickness * 0.5f +
        normal * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position +
        major_axis * (game::constants::grid_cell_stride -
                      game::constants::grid_wall_thickness * 0.5f) -
        minor_axis * game::constants::grid_wall_thickness * 0.5f +
        normal * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position +
        major_axis * game::constants::grid_wall_thickness * 0.5f -
        minor_axis * game::constants::grid_wall_thickness * 0.5f +
        normal * game::constants::grid_wall_thickness * 0.5f,
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

std::pair<std::array<Grid_vertex, 4>, std::array<std::uint32_t, 6>>
generate_vertex_geometry(
  const Eigen::Vector3i &cell_indices,
  const Eigen::Vector3f &u,
  const Eigen::Vector3f &v,
  const Eigen::Vector3f &normal,
  std::uint32_t face_begin_index) {
  const auto cell_position =
    (cell_indices.cast<float>() * game::constants::grid_cell_stride).eval();
  const auto vertices = std::array<Grid_vertex, 4>{
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position + u * game::constants::grid_wall_thickness * 0.5f +
        v * game::constants::grid_wall_thickness * 0.5f +
        normal * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position - u * game::constants::grid_wall_thickness * 0.5f +
        v * game::constants::grid_wall_thickness * 0.5f +
        normal * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position - u * game::constants::grid_wall_thickness * 0.5f -
        v * game::constants::grid_wall_thickness * 0.5f +
        normal * game::constants::grid_wall_thickness * 0.5f,
    },
    Grid_vertex{
      .position =
        Eigen::Vector3f::Constant(game::constants::grid_wall_thickness * 0.5f) +
        cell_position + u * game::constants::grid_wall_thickness * 0.5f -
        v * game::constants::grid_wall_thickness * 0.5f +
        normal * game::constants::grid_wall_thickness * 0.5f,
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

Faces_geometry_generation_result generate_faces_geometry(
  const Eigen::Vector3i &chunk_indices, const game::Chunk &chunk) {
  auto retval = Faces_geometry_generation_result{};
  auto &x_vertices = retval.vertices[0];
  auto &y_vertices = retval.vertices[1];
  auto &z_vertices = retval.vertices[2];
  auto &x_indices = retval.indices[0];
  auto &y_indices = retval.indices[1];
  auto &z_indices = retval.indices[2];
  constexpr auto n = static_cast<int>(game::Chunk::edge_length);
  for (auto x = 0; x != n; ++x) {
    for (auto y = 0; y != n; ++y) {
      for (auto z = 0; z != n; ++z) {
        const auto cell_indices = Eigen::Vector3i{
          chunk_indices.x() * n + x,
          chunk_indices.y() * n + y,
          chunk_indices.z() * n + z,
        };
        if (chunk.is_solid(game::Axis::x, {x, y, z})) {
          const auto [face_vertices, face_indices] = generate_face_geometry(
            cell_indices,
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            static_cast<std::uint32_t>(x_vertices.size()));
          x_vertices.append_range(face_vertices);
          x_indices.append_range(face_indices);
        }
        if (chunk.is_solid(game::Axis::y, {x, y, z})) {
          const auto [face_vertices, face_indices] = generate_face_geometry(
            cell_indices,
            {0.0f, 0.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            static_cast<std::uint32_t>(y_vertices.size()));
          y_vertices.append_range(face_vertices);
          y_indices.append_range(face_indices);
        }
        if (chunk.is_solid(game::Axis::z, {x, y, z})) {
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
  return retval;
}

std::array<std::array<std::array<bool, 2>, 3>, 3>
calculate_edge_visibility_matrix(
  const game::Grid &grid,
  const Eigen::Vector3i &cell_indices,
  bool cell_x_solid,
  bool cell_y_solid,
  bool cell_z_solid) noexcept {
  auto retval = std::array<std::array<std::array<bool, 2>, 3>, 3>{};
  // x-axis edge, +y normal
  retval[0][1][0] =
    !cell_z_solid &&
    (cell_y_solid ||
     grid.is_solid(game::Axis::y, cell_indices - Eigen::Vector3i{0, 0, 1}) ||
     grid.is_solid(game::Axis::z, cell_indices - Eigen::Vector3i{0, 1, 0}));
  // x-axis edge, -y normal
  retval[0][1][1] =
    !grid.is_solid(game::Axis::z, cell_indices - Eigen::Vector3i{0, 1, 0}) &&
    (cell_y_solid || cell_z_solid ||
     grid.is_solid(game::Axis::y, cell_indices - Eigen::Vector3i{0, 0, 1}));
  // x-axis edge, +z normal
  retval[0][2][0] =
    !cell_y_solid &&
    (cell_z_solid ||
     grid.is_solid(game::Axis::z, cell_indices - Eigen::Vector3i{0, 1, 0}) ||
     grid.is_solid(game::Axis::y, cell_indices - Eigen::Vector3i{0, 0, 1}));
  // x-axis edge, -z normal
  retval[0][2][1] =
    !grid.is_solid(game::Axis::y, cell_indices - Eigen::Vector3i{0, 0, 1}) &&
    (cell_z_solid || cell_y_solid ||
     grid.is_solid(game::Axis::z, cell_indices - Eigen::Vector3i{0, 1, 0}));
  // y-axis edge, +x normal
  retval[1][0][0] =
    !cell_z_solid &&
    (cell_x_solid ||
     grid.is_solid(game::Axis::x, cell_indices - Eigen::Vector3i{0, 0, 1}) ||
     grid.is_solid(game::Axis::z, cell_indices - Eigen::Vector3i{1, 0, 0}));
  // y-axis edge, -x normal
  retval[1][0][1] =
    !grid.is_solid(game::Axis::z, cell_indices - Eigen::Vector3i{1, 0, 0}) &&
    (cell_x_solid || cell_z_solid ||
     grid.is_solid(game::Axis::x, cell_indices - Eigen::Vector3i{0, 0, 1}));
  // y-axis edge, +z normal
  retval[1][2][0] =
    !cell_x_solid &&
    (cell_z_solid ||
     grid.is_solid(game::Axis::x, cell_indices - Eigen::Vector3i{0, 0, 1}) ||
     grid.is_solid(game::Axis::z, cell_indices - Eigen::Vector3i{1, 0, 0}));
  // y-axis edge, -z normal
  retval[1][2][1] =
    !grid.is_solid(game::Axis::x, cell_indices - Eigen::Vector3i{0, 0, 1}) &&
    (cell_z_solid || cell_x_solid ||
     grid.is_solid(game::Axis::z, cell_indices - Eigen::Vector3i{1, 0, 0}));
  // z-axis edge, +x normal
  retval[2][0][0] =
    !cell_y_solid &&
    (cell_x_solid ||
     grid.is_solid(game::Axis::y, cell_indices - Eigen::Vector3i{1, 0, 0}) ||
     grid.is_solid(game::Axis::x, cell_indices - Eigen::Vector3i{0, 1, 0}));
  // z-axis edge, -x normal
  retval[2][0][1] =
    !grid.is_solid(game::Axis::y, cell_indices - Eigen::Vector3i{1, 0, 0}) &&
    (cell_x_solid || cell_y_solid ||
     grid.is_solid(game::Axis::x, cell_indices - Eigen::Vector3i{0, 1, 0}));
  // z-axis edge, +y normal
  retval[2][1][0] =
    !cell_x_solid &&
    (cell_y_solid ||
     grid.is_solid(game::Axis::x, cell_indices - Eigen::Vector3i{0, 1, 0}) ||
     grid.is_solid(game::Axis::y, cell_indices - Eigen::Vector3i{1, 0, 0}));
  // z-axis edge, -y normal
  retval[2][1][1] =
    !grid.is_solid(game::Axis::x, cell_indices - Eigen::Vector3i{0, 1, 0}) &&
    (cell_y_solid || cell_x_solid ||
     grid.is_solid(game::Axis::y, cell_indices - Eigen::Vector3i{1, 0, 0}));
  return retval;
}

struct Edges_geometry_generation_result {
  std::array<std::array<std::vector<Grid_vertex>, 2>, 3> vertices;
  std::array<std::array<std::vector<std::uint32_t>, 2>, 3> indices;
};

Edges_geometry_generation_result generate_edges_geometry(
  const Eigen::Vector3i &chunk_indices,
  const game::Chunk &chunk,
  const game::Grid &grid) {
  auto retval = Edges_geometry_generation_result{};
  constexpr auto n = static_cast<int>(game::Chunk::edge_length);
  for (auto x = 0; x != n; ++x) {
    for (auto y = 0; y != n; ++y) {
      for (auto z = 0; z != n; ++z) {
        const auto cell_indices = Eigen::Vector3i{
          chunk_indices.x() * n + x,
          chunk_indices.y() * n + y,
          chunk_indices.z() * n + z,
        };
        const auto x_solid = chunk.is_solid(game::Axis::x, {x, y, z});
        const auto y_solid = chunk.is_solid(game::Axis::y, {x, y, z});
        const auto z_solid = chunk.is_solid(game::Axis::z, {x, y, z});
        const auto visibility_matrix = calculate_edge_visibility_matrix(
          grid, cell_indices, x_solid, y_solid, z_solid);
        if (visibility_matrix[0][1][0]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f},
            retval.vertices[1][0].size());
          retval.vertices[1][0].append_range(vertices);
          retval.indices[1][0].append_range(indices);
        }
        if (visibility_matrix[0][1][1]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, -1.0f, 0.0f},
            retval.vertices[1][1].size());
          retval.vertices[1][1].append_range(vertices);
          retval.indices[1][1].append_range(indices);
        }
        if (visibility_matrix[0][2][0]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {1.0f, 0.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            retval.vertices[2][0].size());
          retval.vertices[2][0].append_range(vertices);
          retval.indices[2][0].append_range(indices);
        }
        if (visibility_matrix[0][2][1]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, -1.0f},
            retval.vertices[2][1].size());
          retval.vertices[2][1].append_range(vertices);
          retval.indices[2][1].append_range(indices);
        }
        if (visibility_matrix[1][0][0]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, -1.0f},
            {1.0f, 0.0f, 0.0f},
            retval.vertices[0][0].size());
          retval.vertices[0][0].append_range(vertices);
          retval.indices[0][0].append_range(indices);
        }
        if (visibility_matrix[1][0][1]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {-1.0f, 0.0f, 0.0f},
            retval.vertices[0][1].size());
          retval.vertices[0][1].append_range(vertices);
          retval.indices[0][1].append_range(indices);
        }
        if (visibility_matrix[1][2][0]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {0.0f, 1.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            retval.vertices[2][0].size());
          retval.vertices[2][0].append_range(vertices);
          retval.indices[2][0].append_range(indices);
        }
        if (visibility_matrix[1][2][1]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {0.0f, 1.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f},
            retval.vertices[2][1].size());
          retval.vertices[2][1].append_range(vertices);
          retval.indices[2][1].append_range(indices);
        }
        if (visibility_matrix[2][0][0]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            retval.vertices[0][0].size());
          retval.vertices[0][0].append_range(vertices);
          retval.indices[0][0].append_range(indices);
        }
        if (visibility_matrix[2][0][1]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {0.0f, 0.0f, 1.0f},
            {0.0f, -1.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f},
            retval.vertices[0][1].size());
          retval.vertices[0][1].append_range(vertices);
          retval.indices[0][1].append_range(indices);
        }
        if (visibility_matrix[2][1][0]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {0.0f, 0.0f, 1.0f},
            {-1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            retval.vertices[1][0].size());
          retval.vertices[1][0].append_range(vertices);
          retval.indices[1][0].append_range(indices);
        }
        // z-axis edge, -y normal
        if (visibility_matrix[2][1][1]) {
          const auto [vertices, indices] = generate_edge_geometry(
            cell_indices,
            {0.0f, 0.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
            retval.vertices[1][1].size());
          retval.vertices[1][1].append_range(vertices);
          retval.indices[1][1].append_range(indices);
        }
        const auto x_neighbor_indices = cell_indices - Eigen::Vector3i{1, 0, 0};
        const auto y_neighbor_indices = cell_indices - Eigen::Vector3i{0, 1, 0};
        const auto z_neighbor_indices = cell_indices - Eigen::Vector3i{0, 0, 1};
        const auto xy_neighbor_indices =
          cell_indices - Eigen::Vector3i{1, 1, 0};
        const auto xz_neighbor_indices =
          cell_indices - Eigen::Vector3i{1, 0, 1};
        const auto yz_neighbor_indices =
          cell_indices - Eigen::Vector3i{0, 1, 1};
        const auto vertex_solid =
          x_solid || y_solid || z_solid ||
          grid.is_solid(game::Axis::y, x_neighbor_indices) ||
          grid.is_solid(game::Axis::z, x_neighbor_indices) ||
          grid.is_solid(game::Axis::x, y_neighbor_indices) ||
          grid.is_solid(game::Axis::z, y_neighbor_indices) ||
          grid.is_solid(game::Axis::x, z_neighbor_indices) ||
          grid.is_solid(game::Axis::y, z_neighbor_indices) ||
          grid.is_solid(game::Axis::z, xy_neighbor_indices) ||
          grid.is_solid(game::Axis::y, xz_neighbor_indices) ||
          grid.is_solid(game::Axis::x, yz_neighbor_indices);
        if (vertex_solid) {
          const auto x_neighbor_visibility_matrix =
            calculate_edge_visibility_matrix(
              grid,
              x_neighbor_indices,
              grid.is_solid(game::Axis::x, x_neighbor_indices),
              grid.is_solid(game::Axis::y, x_neighbor_indices),
              grid.is_solid(game::Axis::z, x_neighbor_indices));
          const auto y_neighbor_visibility_matrix =
            calculate_edge_visibility_matrix(
              grid,
              y_neighbor_indices,
              grid.is_solid(game::Axis::x, y_neighbor_indices),
              grid.is_solid(game::Axis::y, y_neighbor_indices),
              grid.is_solid(game::Axis::z, y_neighbor_indices));
          const auto z_neighbor_visibility_matrix =
            calculate_edge_visibility_matrix(
              grid,
              z_neighbor_indices,
              grid.is_solid(game::Axis::x, z_neighbor_indices),
              grid.is_solid(game::Axis::y, z_neighbor_indices),
              grid.is_solid(game::Axis::z, z_neighbor_indices));
          const auto predicate = [](bool b) { return b; };
          // +x normal
          if (
            std::ranges::none_of(visibility_matrix[0][1], predicate) &&
            std::ranges::none_of(visibility_matrix[0][2], predicate)) {
            const auto [vertices, indices] = generate_vertex_geometry(
              cell_indices,
              {0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f, 1.0f},
              {1.0f, 0.0f, 0.0f},
              retval.vertices[0][0].size());
            retval.vertices[0][0].append_range(vertices);
            retval.indices[0][0].append_range(indices);
          }
          // -x normal
          if (
            std::ranges::none_of(
              x_neighbor_visibility_matrix[0][1], predicate) &&
            std::ranges::none_of(
              x_neighbor_visibility_matrix[0][2], predicate)) {
            const auto [vertices, indices] = generate_vertex_geometry(
              cell_indices,
              {0.0f, -1.0f, 0.0f},
              {0.0f, 0.0f, 1.0f},
              {-1.0f, 0.0f, 0.0f},
              retval.vertices[0][1].size());
            retval.vertices[0][1].append_range(vertices);
            retval.indices[0][1].append_range(indices);
          }
          // +y normal
          if (
            std::ranges::none_of(visibility_matrix[1][0], predicate) &&
            std::ranges::none_of(visibility_matrix[1][2], predicate)) {
            const auto [vertices, indices] = generate_vertex_geometry(
              cell_indices,
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f},
              {0.0f, 1.0f, 0.0f},
              retval.vertices[1][0].size());
            retval.vertices[1][0].append_range(vertices);
            retval.indices[1][0].append_range(indices);
          }
          // -y normal
          if (
            std::ranges::none_of(
              y_neighbor_visibility_matrix[1][0], predicate) &&
            std::ranges::none_of(
              y_neighbor_visibility_matrix[1][2], predicate)) {
            const auto [vertices, indices] = generate_vertex_geometry(
              cell_indices,
              {1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f},
              {0.0f, -1.0f, 0.0f},
              retval.vertices[1][1].size());
            retval.vertices[1][1].append_range(vertices);
            retval.indices[1][1].append_range(indices);
          }
          // +z normal
          if (
            std::ranges::none_of(visibility_matrix[2][0], predicate) &&
            std::ranges::none_of(visibility_matrix[2][1], predicate)) {
            const auto [vertices, indices] = generate_vertex_geometry(
              cell_indices,
              {0.0f, 1.0f, 0.0f},
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f},
              retval.vertices[2][0].size());
            retval.vertices[2][0].append_range(vertices);
            retval.indices[2][0].append_range(indices);
          }
          // -z normal
          if (
            std::ranges::none_of(
              z_neighbor_visibility_matrix[2][0], predicate) &&
            std::ranges::none_of(
              z_neighbor_visibility_matrix[2][1], predicate)) {
            const auto [vertices, indices] = generate_vertex_geometry(
              cell_indices,
              {0.0f, -1.0f, 0.0f},
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, -1.0f},
              retval.vertices[2][1].size());
            retval.vertices[2][1].append_range(vertices);
            retval.indices[2][1].append_range(indices);
          }
        }
      }
    }
  }
  return retval;
}
} // namespace

Grid_mesh::Grid_mesh(const Grid_mesh_create_info &info) {
  auto vertices = std::vector<Grid_vertex>{};
  auto indices = std::vector<std::uint32_t>{};
  auto face_draw_infos =
    std::array<std::vector<graphics::Indexed_draw_info>, 3>{};
  auto edge_draw_infos =
    std::array<std::array<std::vector<graphics::Indexed_draw_info>, 2>, 3>{};
  for (const auto &[chunk_indices, chunk] : info.grid->get_chunks()) {
    const auto faces_geometry = generate_faces_geometry(chunk_indices, *chunk);
    const auto edges_geometry =
      generate_edges_geometry(chunk_indices, *chunk, *info.grid);
    for (auto axis = 0; axis != 3; ++axis) {
      const auto face_draw_info = graphics::Indexed_draw_info{
        .index_count =
          static_cast<std::uint32_t>(faces_geometry.indices[axis].size()),
        .instance_count = 1,
        .first_index = static_cast<std::uint32_t>(indices.size()),
        .vertex_offset = static_cast<std::int32_t>(vertices.size()),
        .first_instance = 0,
      };
      if (face_draw_info.index_count) {
        vertices.append_range(faces_geometry.vertices[axis]);
        indices.append_range(faces_geometry.indices[axis]);
        face_draw_infos[axis].emplace_back(face_draw_info);
      }
      for (auto sign = 0; sign != 2; ++sign) {
        const auto edge_draw_info = graphics::Indexed_draw_info{
          .index_count = static_cast<std::uint32_t>(
            edges_geometry.indices[axis][sign].size()),
          .instance_count = 1,
          .first_index = static_cast<std::uint32_t>(indices.size()),
          .vertex_offset = static_cast<std::int32_t>(vertices.size()),
          .first_instance = 0,
        };
        if (edge_draw_info.index_count) {
          vertices.append_range(edges_geometry.vertices[axis][sign]);
          indices.append_range(edges_geometry.indices[axis][sign]);
          edge_draw_infos[axis][sign].emplace_back(edge_draw_info);
        }
      }
    }
  }
  const auto draw_count = [&]() {
    auto retval = std::uint64_t{};
    for (auto axis = 0; axis != 3; ++axis) {
      retval += face_draw_infos[axis].size();
      for (auto sign = 0; sign != 2; ++sign) {
        retval += edge_draw_infos[axis][sign].size();
      }
    }
    return retval;
  }();
  const auto vertex_buffer_size = vertices.size() * sizeof(Grid_vertex);
  const auto index_buffer_size = indices.size() * sizeof(std::uint32_t);
  const auto draw_buffer_size =
    draw_count * sizeof(graphics::Indexed_draw_info);
  if (vertex_buffer_size > 0 && index_buffer_size > 0 && draw_buffer_size > 0) {
    const auto staging_buffer = info.graphics->create_staging_buffer(
      vertex_buffer_size + index_buffer_size + draw_buffer_size);
    const auto staging_buffer_memory = staging_buffer->map();
    auto staging_buffer_writer =
      serial::Span_writer{staging_buffer_memory.get()};
    staging_buffer_writer.write(std::as_bytes(std::span{vertices}));
    staging_buffer_writer.write(std::as_bytes(std::span{indices}));
    const auto staging_buffer_draw_buffer_begin =
      staging_buffer_writer.offset();
    for (auto axis = 0; axis != 3; ++axis) {
      _indirect_face_draw_infos[axis].offset =
        staging_buffer_writer.offset() - staging_buffer_draw_buffer_begin;
      _indirect_face_draw_infos[axis].draw_count = face_draw_infos[axis].size();
      staging_buffer_writer
        .write(std::as_bytes(std::span{face_draw_infos[axis]}));
    }
    for (auto axis = 0; axis != 3; ++axis) {
      for (auto sign = 0; sign != 2; ++sign) {
        _indirect_edge_draw_infos[axis][sign].offset =
          staging_buffer_writer.offset() - staging_buffer_draw_buffer_begin;
        _indirect_edge_draw_infos[axis][sign].draw_count =
          edge_draw_infos[axis][sign].size();
        staging_buffer_writer
          .write(std::as_bytes(std::span{edge_draw_infos[axis][sign]}));
      }
    }
    _vertex_buffer = info.graphics->create_vertex_buffer(vertex_buffer_size);
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
    recorder.copy_buffer(
      staging_buffer,
      _draw_buffer,
      {
        .src_offset = vertex_buffer_size + index_buffer_size,
        .dst_offset = 0,
        .size = draw_buffer_size,
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

void Grid_mesh::record_face_drawing_commands(
  graphics::Work_recorder &recorder, game::Axis axis) {
  assert(is_uploaded());
  const auto draw_info = _indirect_face_draw_infos[static_cast<int>(axis)];
  recorder.draw_indexed_indirect({
    .buffer = _draw_buffer,
    .offset = draw_info.offset,
    .draw_count = draw_info.draw_count,
    .stride = sizeof(graphics::Indexed_draw_info),
  });
}

void Grid_mesh::record_edge_drawing_commands(
  graphics::Work_recorder &recorder, game::Axis axis, client::Sign sign) {
  assert(is_uploaded());
  const auto draw_info =
    _indirect_edge_draw_infos[static_cast<int>(axis)][static_cast<int>(sign)];
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

const rc::Strong<graphics::Buffer> &
Grid_mesh::get_vertex_buffer() const noexcept {
  return _vertex_buffer;
}

const rc::Strong<graphics::Buffer> &
Grid_mesh::get_index_buffer() const noexcept {
  return _index_buffer;
}

void Grid_mesh::on_work_done(const graphics::Work &) { _upload_work = nullptr; }
} // namespace fpsparty::client
