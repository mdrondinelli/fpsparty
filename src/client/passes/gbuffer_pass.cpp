#include "client/passes/gbuffer_pass.hpp"
#include "client/scene_uniform_layout.hpp"
#include "math/transforms.hpp"
#include "client/rt_math.hpp"
#include "render_graph/access.hpp"
#include "scene/elements.hpp"
#include <Eigen/Dense>
#include <array>
#include <cstring>
#include <span>
#include <utility>

namespace fpsparty::client::passes {

Gbuffer_pass::Gbuffer_pass(
  rc::Strong<graphics::Pipeline> grid_pipeline,
  rc::Strong<graphics::Pipeline> mesh_pipeline,
  std::size_t cube_index_count,
  float z_near)
    : _grid_pipeline{std::move(grid_pipeline)},
      _mesh_pipeline{std::move(mesh_pipeline)},
      _cube_index_count{cube_index_count},
      _z_near{z_near} {}

void Gbuffer_pass::update(Gbuffer_pass_inputs inputs) {
  _inputs = std::move(inputs);
}

void Gbuffer_pass::declare(render_graph::Builder &builder) {
  _albedo_handle = builder.write(
    _inputs.albedo_render_target, render_graph::access::color_attachment_write);
  _normal_handle = builder.write(
    _inputs.normal_render_target, render_graph::access::color_attachment_write);
  _motion_vector_handle = builder.write(
    _inputs.motion_vector_render_target,
    render_graph::access::color_attachment_write);
  _depth_gradient_handle = builder.write(
    _inputs.depth_gradient_render_target,
    render_graph::access::color_attachment_write);
  _depth_handle = builder.write(
    _inputs.depth_render_target, render_graph::access::depth_attachment_write);
}

void Gbuffer_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const gbuffer_color_attachments = std::array{
    graphics::Color_attachment_info{.image = resources.get_image(_albedo_handle)},
    graphics::Color_attachment_info{.image = resources.get_image(_normal_handle)},
    graphics::Color_attachment_info{
      .image = resources.get_image(_motion_vector_handle)},
    graphics::Color_attachment_info{
      .image = resources.get_image(_depth_gradient_handle)},
  };
  recorder.begin_rendering({
    .color_attachments = gbuffer_color_attachments,
    .depth_image = resources.get_image(_depth_handle),
  });
  recorder.set_viewport(_inputs.framebuffer_size);
  recorder.set_scissor(_inputs.framebuffer_size);
  auto const &session = _inputs.client->get_session();
  auto const camera =
    session && _inputs.local_player &&
        _inputs.local_player->player_entity_id &&
        _inputs.local_player->humanoid_entity_id
      ? session->get_scene().get_camera(*_inputs.local_player->player_entity_id)
      : nullptr;
  recorder.set_depth_test_enabled(true);
  recorder.set_depth_write_enabled(true);
  recorder.set_depth_compare_op(graphics::Compare_op::greater);
  if (camera) {
    auto const view_matrix =
      (math::x_rotation_matrix(-_inputs.local_player->input_state.pitch) *
       math::y_rotation_matrix(-_inputs.local_player->input_state.yaw) *
       math::translation_matrix(-camera->position))
        .eval();
    auto const zoom = 1.125f;
    auto const aspect_ratio =
      static_cast<float>(_inputs.framebuffer_size.x()) /
      static_cast<float>(_inputs.framebuffer_size.y());
    auto const zoom_vec = math::vec2{
      aspect_ratio > 1.0f ? zoom : zoom * aspect_ratio,
      aspect_ratio > 1.0f ? zoom / aspect_ratio : zoom,
    };
    auto const projection_matrix =
      math::perspective_projection_matrix(zoom_vec.x(), zoom_vec.y(), _z_near);
    auto const view_projection_matrix = (projection_matrix * view_matrix).eval();
    auto const camera_basis =
      (math::translation_matrix(camera->position) *
       math::y_rotation_matrix(_inputs.local_player->input_state.yaw) *
       math::x_rotation_matrix(_inputs.local_player->input_state.pitch))
        .eval();
    auto const camera_basis_rows = make_rt_matrix_rows(camera_basis);
    auto const scene_uniform_memory = _inputs.scene_uniform_buffer->map();
    auto const write_scene_uniform =
      [&]<typename T>(std::size_t offset, T const &value) {
        std::memcpy(
          scene_uniform_memory.get().data() + _inputs.scene_uniform_offset +
            offset,
          &value,
          sizeof(value));
      };
    auto const previous_view_projection_matrix =
      _previous_view_projection_matrix.value_or(view_projection_matrix);
    _previous_view_projection_matrix = view_projection_matrix;
    write_scene_uniform(
      scene_view_projection_matrix_offset, view_projection_matrix);
    write_scene_uniform(
      scene_previous_view_projection_matrix_offset,
      previous_view_projection_matrix);
    write_scene_uniform(scene_animation_time_offset, _inputs.animation_time);
    write_scene_uniform(scene_camera_basis_offset, camera_basis_rows);
    write_scene_uniform(scene_zoom_offset, zoom_vec);
    write_scene_uniform(scene_z_near_offset, _z_near);
    auto const sun =
      session->get_scene().get_distant_light(scene::elements::sun_light_key);
    if (sun) {
      write_scene_uniform(scene_sun_direction_offset, sun->direction);
      write_scene_uniform(scene_sun_irradiance_offset, sun->irradiance);
    }
    // draw grid
    if (_inputs.grid_mesh && _inputs.grid_mesh->is_uploaded()) {
      recorder.bind_pipeline(_grid_pipeline);
      recorder.set_front_face(graphics::Front_face::counter_clockwise);
      recorder.set_cull_mode(graphics::Cull_mode::back);
      recorder.bind_index_buffer(
        _inputs.grid_mesh->get_index_buffer(), graphics::Index_type::u32);
      recorder.push_buffer_reference(
        0, _inputs.scene_uniform_buffer, _inputs.scene_uniform_offset);
      recorder.push_buffer_reference(8, _inputs.grid_mesh->get_vertex_buffer());
      recorder.push_buffer_reference(
        16, _inputs.block_texture_registry->get_buffer());
      _inputs.block_texture_registry->add_references(recorder);
      auto push_normal = [&](math::vec3 const &value) {
        recorder.push_data(24, std::as_bytes(std::span{&value, 1}));
      };
      push_normal({1.0f, 0.0f, 0.0f});
      _inputs.grid_mesh->record_draws(recorder, +math::axis3::x);
      push_normal({-1.0f, 0.0f, 0.0f});
      _inputs.grid_mesh->record_draws(recorder, -math::axis3::x);
      push_normal({0.0f, 1.0f, 0.0f});
      _inputs.grid_mesh->record_draws(recorder, +math::axis3::y);
      push_normal({0.0f, -1.0f, 0.0f});
      _inputs.grid_mesh->record_draws(recorder, -math::axis3::y);
      push_normal({0.0f, 0.0f, 1.0f});
      _inputs.grid_mesh->record_draws(recorder, +math::axis3::z);
      push_normal({0.0f, 0.0f, -1.0f});
      _inputs.grid_mesh->record_draws(recorder, -math::axis3::z);
    }
    // draw cubes
    recorder.bind_pipeline(_mesh_pipeline);
    recorder.set_cull_mode(graphics::Cull_mode::back);
    recorder.set_front_face(graphics::Front_face::counter_clockwise);
    recorder.bind_index_buffer(
      _inputs.cube_index_buffer, graphics::Index_type::u16);
    recorder.push_buffer_reference(
      0, _inputs.scene_uniform_buffer, _inputs.scene_uniform_offset);
    recorder.push_buffer_reference(8, _inputs.cube_vertex_buffer);
    for (auto const &box : session->get_scene().get_current_frame().boxes) {
      auto const local_body_key = _inputs.local_player->humanoid_entity_id
                                    ? scene::elements::entity_part_key(
                                        *_inputs.local_player->humanoid_entity_id,
                                        0)
                                    : 0;
      auto const local_head_key = _inputs.local_player->humanoid_entity_id
                                    ? scene::elements::entity_part_key(
                                        *_inputs.local_player->humanoid_entity_id,
                                        1)
                                    : 0;
      if (box.key == local_body_key || box.key == local_head_key) {
        continue;
      }
      auto const previous_box = session->get_scene().get_previous_box(box.key);
      auto const model_matrix = make_model_matrix(box);
      auto const previous_model_matrix =
        make_model_matrix(previous_box ? *previous_box : box);
      auto const model_rows = Eigen::Matrix<float, 3, 4, Eigen::RowMajor>{
        model_matrix.topRows<3>()};
      auto const previous_model_rows =
        Eigen::Matrix<float, 3, 4, Eigen::RowMajor>{
          previous_model_matrix.topRows<3>()};
      recorder.push_data(16, std::as_bytes(std::span{&model_rows, 1}));
      recorder.push_data(64, std::as_bytes(std::span{&previous_model_rows, 1}));
      recorder.draw_indexed({
        .index_count = static_cast<std::uint32_t>(_cube_index_count),
        .instance_count = 1,
        .first_index = 0,
        .vertex_offset = 0,
        .first_instance = 0,
      });
    }
  }
  recorder.end_rendering();
}

} // namespace fpsparty::client::passes
