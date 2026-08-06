#include "client/passes/radiance_pass.hpp"
#include "math/transforms.hpp"
#include "render_graph/access.hpp"
#include <Eigen/Dense>
#include <array>
#include <span>
#include <utility>

namespace fpsparty::client::passes {

namespace {
auto const sky_color = math::vec4{0.4196f, 0.6196f, 0.7451f, 1.0f};
} // namespace

Radiance_pass::Radiance_pass(
  rc::Strong<graphics::Compute_pipeline> pipeline, Radiance_pass_inputs inputs)
    : _pipeline{std::move(pipeline)}, _inputs{std::move(inputs)} {}

bool Radiance_pass::has_camera() const {
  auto const &session = _inputs.client->get_session();
  return session && _inputs.local_player &&
         _inputs.local_player->player_entity_id &&
         _inputs.local_player->humanoid_entity_id &&
         session->get_scene().get_camera(*_inputs.local_player->player_entity_id) &&
         _inputs.grid_mesh && _inputs.grid_mesh->is_uploaded();
}

void Radiance_pass::declare(render_graph::Builder &builder) {
  if (has_camera()) {
    // albedo/depth_normal: written by Gbuffer_pass. sky_view_lut: written
    // by Sky_view_pass, if it ran this frame (its own gating -- camera &&
    // sun -- can be true even when this pass's gating -- camera &&
    // grid_mesh uploaded -- is, without sun; declaring the read
    // regardless is harmless, see Distant_irradiance_pass1::declare's
    // comment). distant_irradiance_filtered: written by the last
    // Distant_irradiance_spatial_filter_pass iteration.
    _albedo_handle = builder.read(
      _inputs.albedo_render_target, render_graph::access::compute_sampled_read);
    _depth_normal_handle = builder.read(
      _inputs.depth_normal_render_target,
      render_graph::access::compute_sampled_read);
    _sky_view_lut_handle = builder.read(
      _inputs.sky_view_lut, render_graph::access::compute_sampled_read);
    _distant_irradiance_filtered_handle = builder.read(
      _inputs.distant_irradiance_filtered,
      render_graph::access::compute_sampled_read);
  }
  _radiance_handle = builder.write(
    _inputs.radiance_render_target,
    has_camera() ? render_graph::access::compute_storage_write
                 : render_graph::access::color_attachment_write);
}

void Radiance_pass::execute(
  graphics::Work_recorder &recorder, render_graph::Resources &resources) {
  auto const &session = _inputs.client->get_session();
  auto const camera =
    has_camera()
      ? session->get_scene().get_camera(*_inputs.local_player->player_entity_id)
      : nullptr;
  if (!camera) {
    // No camera yet, or the grid mesh (and its shadow voxel buffer the
    // shadow raymarch depends on) hasn't finished uploading: nothing was
    // rasterized into the G-buffer this frame, so just clear the radiance
    // target directly instead of dispatching a lighting pass with no
    // valid view or grid to light with.
    auto const radiance_color_attachments = std::array{
      graphics::Color_attachment_info{
        .image = resources.get_image(_radiance_handle), .clear_value = sky_color},
    };
    recorder.begin_rendering({.color_attachments = radiance_color_attachments});
    recorder.end_rendering();
    return;
  }
  auto constexpr zoom = 1.125f;
  auto const aspect_ratio =
    static_cast<float>(_inputs.framebuffer_size.x()) /
    static_cast<float>(_inputs.framebuffer_size.y());
  auto const zoom_vec = math::vec2{
    aspect_ratio > 1.0f ? zoom : zoom * aspect_ratio,
    aspect_ratio > 1.0f ? zoom / aspect_ratio : zoom,
  };
  auto const camera_basis =
    (math::translation_matrix(camera->position) *
     math::y_rotation_matrix(_inputs.local_player->input_state.yaw) *
     math::x_rotation_matrix(_inputs.local_player->input_state.pitch))
      .eval();
  // Rows contiguous, matching the row_major mat4x3 push constants.
  auto const camera_basis_rows =
    Eigen::Matrix<float, 3, 4, Eigen::RowMajor>{camera_basis.topRows<3>()};
  recorder.bind_compute_pipeline(_pipeline);
  recorder.push_data(0, std::as_bytes(std::span{&camera_basis_rows, 1}));
  recorder.push_data(48, std::as_bytes(std::span{&zoom_vec, 1}));
  recorder.push_descriptors(
    56,
    {{.image = resources.get_image(_albedo_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_depth_normal_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_sky_view_lut_handle),
      .kind = graphics::Descriptor_kind::sampled},
     {.image = resources.get_image(_radiance_handle),
      .kind = graphics::Descriptor_kind::storage},
     {.image = resources.get_image(_distant_irradiance_filtered_handle),
      .kind = graphics::Descriptor_kind::sampled}});
  auto const group_count_x =
    static_cast<std::uint32_t>((_inputs.framebuffer_size.x() + 7) / 8);
  auto const group_count_y =
    static_cast<std::uint32_t>((_inputs.framebuffer_size.y() + 7) / 8);
  recorder.dispatch(group_count_x, group_count_y, 1);
}

} // namespace fpsparty::client::passes
