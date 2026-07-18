#include "grid_mesh.hpp"

#include <array>
#include <cassert>
#include <span>
#include <vector>

#include <tracy/Tracy.hpp>

#include <game/grid.hpp>
#include <serial/span_writer.hpp>

#include "grid_vertex.hpp"

namespace fpsparty::client {

namespace {

struct Rt_block_grid_cell {
  std::uint8_t model_index;
  std::uint8_t albedo_index;
};

static_assert(sizeof(Rt_block_grid_cell) == 2);

struct Rt_block_grid_chunk {
  std::array<Rt_block_grid_cell, 64> cells;
};

} // namespace

Grid_mesh::Grid_mesh(Grid_mesh_create_info const &info) {
  ZoneScoped;
  auto const get_block_model = [&](math::ivec3 coords) {
    auto const [block, data] = info.grid->get_block(coords);
    return info.block_model_registry->get(block, data);
  };
  auto vertices = std::vector<Grid_vertex>{};
  auto indices = std::vector<std::uint32_t>{};
  auto draw_infos =
    std::array<std::array<std::vector<graphics::Indexed_draw_info>, 2>, 3>{};
  auto const grid_empty = info.grid->empty();
  auto const chunk_counts =
    grid_empty ? math::ivec3::Zero().eval() : info.grid->get_chunk_counts();
  auto rt_block_grid_chunks = std::vector<Rt_block_grid_chunk>(
    static_cast<std::size_t>(chunk_counts.x()) *
    static_cast<std::size_t>(chunk_counts.y()) *
    static_cast<std::size_t>(chunk_counts.z()));
  for (auto const &[chunk_coords, chunk] : info.grid->get_chunks()) {
    auto const chunk_index = info.grid->get_chunk_index(chunk_coords);
    auto chunk_vertices =
      std::array<std::array<std::vector<Grid_vertex>, 2>, 3>{};
    auto chunk_indices = std::array<std::array<std::vector<u32>, 2>, 3>{};
    constexpr auto n = static_cast<int>(game::Chunk::edge_length);
    for (auto rel_x = 0; rel_x != n; ++rel_x) {
      auto const x = rel_x + chunk_coords.x() * n;
      for (auto rel_y = 0; rel_y != n; ++rel_y) {
        auto const y = rel_y + chunk_coords.y() * n;
        for (auto rel_z = 0; rel_z != n; ++rel_z) {
          auto const z = rel_z + chunk_coords.z() * n;
          auto const [block, data] = chunk->get_block({rel_x, rel_y, rel_z});
          auto const block_model = info.block_model_registry->get(block, data);
          if (!block_model) {
            continue;
          }
          auto const block_coords = math::ivec3{x, y, z};
          for (auto const &axis :
               {math::axis3::x, math::axis3::y, math::axis3::z}) {
            for (auto const &sign :
                 {math::sign::positive, math::sign::negative}) {
              auto &aligned_chunk_vertices =
                chunk_vertices[static_cast<int>(axis)][static_cast<int>(sign)];
              auto &aligned_chunk_indices =
                chunk_indices[static_cast<int>(axis)][static_cast<int>(sign)];
              auto const &aligned_submesh =
                block_model->mesh.aligned_submesh({axis, sign});
              for (auto const &face : aligned_submesh.faces) {
                if (face.occluder) {
                  auto const occluder_model = get_block_model(
                    block_coords + static_cast<math::ivec3>(*face.occluder));
                  if (
                    occluder_model &&
                    occluder_model->occludes_neighbor(-*face.occluder)) {
                    continue;
                  }
                }
                auto const first_index =
                  static_cast<u32>(aligned_chunk_vertices.size());
                for (auto i = 0; i != 4; ++i) {
                  aligned_chunk_vertices.emplace_back(face.vertices[i]);
                  aligned_chunk_vertices.back().position +=
                    block_coords.cast<f32>();
                }
                for (auto const i : {0, 1, 2, 2, 3, 0}) {
                  aligned_chunk_indices.emplace_back(first_index + i);
                }
              }
            }
          }
          if (chunk->is_solid({rel_x, rel_y, rel_z})) {
            auto const block_index =
              game::Chunk::get_block_index({rel_x, rel_y, rel_z});
            switch (block) {
            default:
              rt_block_grid_chunks[chunk_index]
                .cells[block_index]
                .model_index = 1;
              break;
            case game::Block::conveyor:
              rt_block_grid_chunks[chunk_index]
                .cells[block_index]
                .model_index = 2;
              break;
            }
            switch (block) {
            default:
              rt_block_grid_chunks[chunk_index]
                .cells[block_index]
                .albedo_index = 0;
              break;
            case game::Block::dirt:
              rt_block_grid_chunks[chunk_index]
                .cells[block_index]
                .albedo_index = 1;
              break;
            }
          }
        }
      }
    }
    for (auto axis = 0; axis != 3; ++axis) {
      for (auto sign = 0; sign != 2; ++sign) {
        auto const draw_info = graphics::Indexed_draw_info{
          .index_count =
            static_cast<std::uint32_t>(chunk_indices[axis][sign].size()),
          .instance_count = 1,
          .first_index = static_cast<std::uint32_t>(indices.size()),
          .vertex_offset = static_cast<std::int32_t>(vertices.size()),
          .first_instance = 0,
        };
        if (draw_info.index_count) {
          vertices.append_range(chunk_vertices[axis][sign]);
          indices.append_range(chunk_indices[axis][sign]);
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
  auto const chunk_min = grid_empty ? math::ivec3::Zero().eval()
                                    : info.grid->get_chunk_bounds().min();
  auto const rt_block_grid_header = std::array<std::int32_t, 6>{
    chunk_min.x(),
    chunk_min.y(),
    chunk_min.z(),
    chunk_counts.x(),
    chunk_counts.y(),
    chunk_counts.z(),
  };
  static_assert(sizeof(rt_block_grid_header) == 24);
  _rt_block_grid_chunk_count =
    static_cast<std::uint32_t>(rt_block_grid_chunks.size());
  auto const rt_block_grid_buffer_size = sizeof(rt_block_grid_header) +
    sizeof(Rt_block_grid_chunk) * rt_block_grid_chunks.size();
  if (vertex_buffer_size > 0 && index_buffer_size > 0 && draw_buffer_size > 0) {
    auto const staging_buffer = info.graphics->create_staging_buffer(
      vertex_buffer_size + index_buffer_size + draw_buffer_size +
      rt_block_grid_buffer_size);
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
    auto const rt_block_grid_buffer_offset = staging_buffer_writer.offset();
    staging_buffer_writer
      .write(std::as_bytes(std::span{rt_block_grid_header}));
    staging_buffer_writer
      .write(std::as_bytes(std::span{rt_block_grid_chunks}));
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
    _rt_block_grid_buffer = info.graphics->create_buffer({
      .size = rt_block_grid_buffer_size,
      .usage = graphics::Buffer_usage_flag_bits::transfer_dst |
               graphics::Buffer_usage_flag_bits::shader_device_address,
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
    recorder.copy_buffer(
      staging_buffer,
      _rt_block_grid_buffer,
      {
        .src_offset = rt_block_grid_buffer_offset,
        .dst_offset = 0,
        .size = rt_block_grid_buffer_size,
      });
    recorder.barrier(
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
        .access_mask = graphics::Access_flag_bits::transfer_write,
      },
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::draw_indirect |
                      graphics::Pipeline_stage_flag_bits::index_input |
                      graphics::Pipeline_stage_flag_bits::vertex_shader |
                      graphics::Pipeline_stage_flag_bits::compute_shader,
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
  graphics::Work_recorder &recorder, math::signed_axis3 normal) {
  assert(is_uploaded());
  auto const draw_info =
    _draw_infos[static_cast<int>(normal.axis)][static_cast<int>(normal.sign)];
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

rc::Strong<graphics::Buffer> const &
Grid_mesh::get_rt_block_grid_buffer() const noexcept {
  return _rt_block_grid_buffer;
}

std::uint32_t Grid_mesh::get_rt_chunk_count() const noexcept {
  return _rt_block_grid_chunk_count;
}

void Grid_mesh::on_work_done(graphics::Work const &) { _upload_work = nullptr; }
} // namespace fpsparty::client
