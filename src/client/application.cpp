#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.hpp>

#include <constants.hpp>
#include <flt.hpp>
#include <glfw.hpp>
#include <graphics/global_vulkan_state.hpp>
#include <graphics/graphics.hpp>
#include <graphics/pipeline.hpp>
#include <graphics/shader_stage.hpp>
#include <graphics/synchronization_scope.hpp>
#include <graphics/work_done_callback.hpp>
#include <math/transforms.hpp>
#include <math/vec.hpp>
#include <ppm/ppm.hpp>

#include "application.hpp"
#include "block_mod/block_mod.hpp"
#include "block_mod/conveyor.hpp"
#include "block_mod/dirt.hpp"
#include "block_mod/light.hpp"
#include "block_mod/placeholder.hpp"
#include "block_mod/stone.hpp"
#include "block_model_registry.hpp"
#include "block_texture_registry.hpp"
#include "client.hpp"
#include "grid_mesh.hpp"
#include "texture_manager.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace fpsparty::client {

namespace {

struct Vertex {
  float px;
  float py;
  float pz;
  float nx{};
  float ny{};
  float nz{};
};

auto const cube_mesh_vertices = std::vector<Vertex>{
  // +x face
  {.px = 0.5f, .py = 0.5f, .pz = 0.5f, .nx = 1.0f},   // 1
  {.px = 0.5f, .py = -0.5f, .pz = 0.5f, .nx = 1.0f},  // 2
  {.px = 0.5f, .py = 0.5f, .pz = -0.5f, .nx = 1.0f},  // 3
  {.px = 0.5f, .py = -0.5f, .pz = -0.5f, .nx = 1.0f}, // 4
  // -x face
  {.px = -0.5f, .py = 0.5f, .pz = -0.5f, .nx = -1.0f},  // 1
  {.px = -0.5f, .py = -0.5f, .pz = -0.5f, .nx = -1.0f}, // 2
  {.px = -0.5f, .py = 0.5f, .pz = 0.5f, .nx = -1.0f},   // 3
  {.px = -0.5f, .py = -0.5f, .pz = 0.5f, .nx = -1.0f},  // 4
  // +y face
  {.px = 0.5f, .py = 0.5f, .pz = 0.5f, .ny = 1.0f},   // 1
  {.px = 0.5f, .py = 0.5f, .pz = -0.5f, .ny = 1.0f},  // 2
  {.px = -0.5f, .py = 0.5f, .pz = 0.5f, .ny = 1.0f},  // 3
  {.px = -0.5f, .py = 0.5f, .pz = -0.5f, .ny = 1.0f}, // 4
  // -y face
  {.px = 0.5f, .py = -0.5f, .pz = -0.5f, .ny = -1.0f},  // 2
  {.px = 0.5f, .py = -0.5f, .pz = 0.5f, .ny = -1.0f},   // 1
  {.px = -0.5f, .py = -0.5f, .pz = -0.5f, .ny = -1.0f}, // 4
  {.px = -0.5f, .py = -0.5f, .pz = 0.5f, .ny = -1.0f},  // 3
  // +z face
  {.px = -0.5f, .py = 0.5f, .pz = 0.5f, .nz = 1.0f},  // 3
  {.px = -0.5f, .py = -0.5f, .pz = 0.5f, .nz = 1.0f}, // 4
  {.px = 0.5f, .py = 0.5f, .pz = 0.5f, .nz = 1.0f},   // 1
  {.px = 0.5f, .py = -0.5f, .pz = 0.5f, .nz = 1.0f},  // 2
  // -z face
  {.px = 0.5f, .py = 0.5f, .pz = -0.5f, .nz = -1.0f},   // 1
  {.px = 0.5f, .py = -0.5f, .pz = -0.5f, .nz = -1.0f},  // 2
  {.px = -0.5f, .py = 0.5f, .pz = -0.5f, .nz = -1.0f},  // 3
  {.px = -0.5f, .py = -0.5f, .pz = -0.5f, .nz = -1.0f}, // 4
};

auto const cube_mesh_indices = std::vector<std::uint16_t>{
  // +x face
  0,
  1,
  2,
  3,
  2,
  1,
  // -x face
  4,
  5,
  6,
  7,
  6,
  5,
  // +y face
  8,
  9,
  10,
  11,
  10,
  9,
  // -y face
  12,
  13,
  14,
  15,
  14,
  13,
  // +z face
  16,
  17,
  18,
  19,
  18,
  17,
  // -z face
  20,
  21,
  22,
  23,
  22,
  21,
};

auto const crosshair_indices =
  std::array<std::uint16_t, 12>{0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};
auto const composite_indices = std::array<std::uint16_t, 3>{0, 1, 2};
auto const sky_color = math::vec4{0.4196f, 0.6196f, 0.7451f, 1.0f};
auto constexpr z_near = 0.1f;
auto const transmittance_lut_size = math::ivec2{256, 128};
auto const sky_view_lut_size = math::ivec2{256, 256};

auto constexpr scene_uniform_data_size = std::size_t{240};
auto constexpr scene_view_projection_matrix_offset = std::size_t{0};
auto constexpr scene_previous_view_projection_matrix_offset = std::size_t{64};
auto constexpr scene_animation_time_offset = std::size_t{128};
auto constexpr scene_camera_basis_offset = std::size_t{144};
auto constexpr scene_sun_direction_offset = std::size_t{192};
auto constexpr scene_sun_irradiance_offset = std::size_t{208};
auto constexpr scene_zoom_offset = std::size_t{224};
auto constexpr scene_z_near_offset = std::size_t{232};

vk::UniqueSurfaceKHR make_vk_surface(glfw::Window window) {
  auto retval = glfw::create_window_surface_unique(
    graphics::Global_vulkan_state::get().instance(), window);
  std::cout << "Created VkSurfaceKHR.\n";
  return retval;
}

std::vector<std::byte> load_file(char const *path) {
  auto file = std::ifstream{path, std::ios::binary | std::ios::ate};
  if (!file) {
    throw std::runtime_error{std::string{"Failed to open file: "} + path};
  }
  auto const size = file.tellg();
  if (size < 0) {
    throw std::runtime_error{std::string{"Failed to size file: "} + path};
  }
  auto data = std::vector<std::byte>(static_cast<std::size_t>(size));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
    throw std::runtime_error{std::string{"Failed to read file: "} + path};
  }
  return data;
}

auto constexpr max_frames_in_flight = 2;
auto constexpr rt_entity_node_size = 2 * sizeof(std::uint32_t);
auto constexpr rt_entity_nodes_per_chunk = std::size_t{1000};
auto constexpr rt_cells_per_chunk = std::size_t{64};

struct alignas(16) Rt_entity {
  std::array<float, 12> model;
  std::array<float, 12> inverse_model;
  math::vec4 half_extents;
  math::vec4 albedo;
};

struct Rt_entity_binning_buffer_layout {
  std::size_t grid_offset;
  std::size_t nodes_offset;
  std::size_t size;
};

Rt_entity_binning_buffer_layout
make_rt_entity_binning_buffer_layout(std::size_t chunk_count) {
  auto const grid_offset = std::size_t{};
  auto const grid_size =
    chunk_count * rt_cells_per_chunk * sizeof(std::int32_t);
  auto const nodes_offset = grid_offset + grid_size;
  assert(nodes_offset % 8 == 0);
  auto const nodes_size = sizeof(std::uint32_t) + chunk_count *
                                                    rt_entity_nodes_per_chunk *
                                                    rt_entity_node_size;
  return {
    .grid_offset = grid_offset,
    .nodes_offset = nodes_offset,
    .size = nodes_offset + nodes_size,
  };
}

static_assert(sizeof(Rt_entity) == 128);
static_assert(offsetof(Rt_entity, inverse_model) == 48);
static_assert(offsetof(Rt_entity, half_extents) == 96);
static_assert(offsetof(Rt_entity, albedo) == 112);

auto make_rt_matrix_rows(math::mat4 const &matrix) {
  auto rows = std::array<float, 12>{};
  for (auto row = 0; row != 3; ++row) {
    for (auto column = 0; column != 4; ++column) {
      rows[static_cast<std::size_t>(row * 4 + column)] = matrix(row, column);
    }
  }
  return rows;
}

math::mat4 make_model_matrix(scene::elements::Box const &box) {
  auto rotation = math::mat4::Identity().eval();
  rotation.block<3, 3>(0, 0) = box.orientation.toRotationMatrix();
  return (math::translation_matrix(box.position) * rotation *
          math::axis_aligned_scale_matrix(box.half_extents * 2.0f))
    .eval();
}

} // namespace

class Application::Impl : public glfw::Key_callback,
                          public glfw::Mouse_button_callback,
                          public glfw::Cursor_pos_callback {
public:
  explicit Impl(Application_create_info const &create_info)
      : _client{create_info.client_info},
        _server_address{create_info.server_address},
        _glfw_window{glfw::create_window_unique(create_info.window_info)},
        _vk_surface{make_vk_surface(*_glfw_window)},
        _graphics{{
          .window = *_glfw_window,
          .surface = *_vk_surface,
          .vsync_preferred = true,
          .max_frames_in_flight = max_frames_in_flight,
        }},
        _grid_vertex_shader{
          graphics::load_shader("./assets/shaders/grid.vert.spv")},
        _grid_fragment_shader{
          graphics::load_shader("./assets/shaders/grid.frag.spv")},
        _mesh_vertex_shader{
          graphics::load_shader("./assets/shaders/shader.vert.spv")},
        _mesh_fragment_shader{
          graphics::load_shader("./assets/shaders/shader.frag.spv")},
        _crosshair_vertex_shader{
          graphics::load_shader("./assets/shaders/crosshair.vert.spv")},
        _crosshair_fragment_shader{
          graphics::load_shader("./assets/shaders/crosshair.frag.spv")},
        _composite_vertex_shader{
          graphics::load_shader("./assets/shaders/composite.vert.spv")},
        _composite_fragment_shader{
          graphics::load_shader("./assets/shaders/composite.frag.spv")},
        _sky_view_compute_shader{graphics::load_shader(
          "./assets/shaders/atmosphere/sky_view.comp.spv")},
        _direct_radiance_compute_shader{
          graphics::load_shader("./assets/shaders/direct_radiance.comp.spv")},
        _rt_grid_entity_binning_compute_shader{graphics::load_shader(
          "./assets/shaders/bin_rt_grid_entities.comp.spv")},
        _rng_seed_compute_shader{
          graphics::load_shader("./assets/shaders/rng_seed.comp.spv")},
        _radiance_compute_shader{
          graphics::load_shader("./assets/shaders/radiance.comp.spv")},
        _indirect_radiance_compute_shader{
          graphics::load_shader("./assets/shaders/indirect_radiance.comp.spv")},
        _indirect_irradiance_compute_shader{graphics::load_shader(
          "./assets/shaders/indirect_irradiance.comp.spv")},
        _grid_pipeline{make_grid_pipeline()},
        _mesh_pipeline{make_mesh_pipeline()},
        _crosshair_pipeline{make_crosshair_pipeline()},
        _sky_view_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_sky_view_compute_shader})},
        _direct_radiance_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_direct_radiance_compute_shader})},
        _rt_grid_entity_binning_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_rt_grid_entity_binning_compute_shader})},
        _rng_seed_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_rng_seed_compute_shader})},
        _radiance_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_radiance_compute_shader})},
        _indirect_radiance_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_indirect_radiance_compute_shader})},
        _indirect_irradiance_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_indirect_irradiance_compute_shader})},
        _texture_manager{{.graphics = &_graphics}},
        _block_texture_registry{{.graphics = &_graphics}},
        _scene_uniform_buffer{_graphics.create_buffer({
          .size = max_frames_in_flight * scene_uniform_data_size,
          .usage = graphics::Buffer_usage_flag_bits::shader_device_address,
          .mapping_mode = graphics::Mapping_mode::write_only,
          .min_alignment = 16,
        })} {
    std::cout << "Opened window.\n";
    // _graphics.set_vsync_preferred(false);
    _glfw_window->set_key_callback(this);
    _glfw_window->set_mouse_button_callback(this);
    _glfw_window->set_cursor_pos_callback(this);
    init_transmittance_lut();
    init_sky_view_lut();
    _cube_vertex_buffer =
      upload_vertices(std::as_bytes(std::span{cube_mesh_vertices}));
    std::cout << "Uploaded cube vertex buffer.\n";
    _cube_index_buffer =
      upload_indices(std::as_bytes(std::span{cube_mesh_indices}));
    std::cout << "Uploaded cube index buffer.\n";
    _crosshair_index_buffer =
      upload_indices(std::as_bytes(std::span{crosshair_indices}));
    std::cout << "Uploaded crosshair index buffer.\n";
    _composite_index_buffer =
      upload_indices(std::as_bytes(std::span{composite_indices}));
    std::cout << "Uploaded composite index buffer.\n";
    auto const block_mod_init_info = Block_mod_init_info{
      .texture_manager = &_texture_manager,
      .texture_registry = &_block_texture_registry,
      .model_registry = &_block_model_registry,
    };
    Placeholder_block_mod{}.init(block_mod_init_info);
    Stone_block_mod{}.init(block_mod_init_info);
    Dirt_block_mod{}.init(block_mod_init_info);
    Conveyor_block_mod{}.init(block_mod_init_info);
    Light_block_mod{}.init(block_mod_init_info);
    for (auto i = std::size_t{}; i != blue_noise_texture_count; ++i) {
      _blue_noise_texture_descriptors[i] =
        _graphics
          .create_sampled_image_descriptor(_texture_manager.get_blue_noise(i));
    }
  }

  ~Impl() {
    _glfw_window->set_key_callback(nullptr);
    _glfw_window->set_mouse_button_callback(nullptr);
    _glfw_window->set_cursor_pos_callback(nullptr);
  }

  bool update(float duration) {
    assert(duration > 0.0f);
    if (_state == State::stopped || _glfw_window->should_close()) {
      _state = State::stopped;
      return false;
    }
    if (_state == State::initial) {
      _client.connect(_server_address);
      _state = State::connecting;
    }
    _client.poll_events();
    if (_state == State::connecting) {
      if (_client.is_connected()) {
        _state = State::connected;
        _local_player = &_client.get_session()->join_player();
      } else if (!_client.is_connecting()) {
        _state = State::stopped;
        return false;
      }
    }
    _graphics.poll_works();
    if (_state == State::connected) {
      if (!_client.is_connected()) {
        _state = State::stopped;
        return false;
      }
      _client.update(duration);
      update_grid_mesh();
      _animation_time += duration;
    }
    render();
    ++_frame_number;
    return true;
  }

  void exit() { _graphics.wait_idle(); }

private:
  enum class State {
    // Initial state: issues connect and switches to connecting.
    initial,

    // Connecting: polls connection status, switching to connected or stopped.
    connecting,

    // Client connected. Says nothing about state of the players.
    connected,

    // Stopped.
    stopped,
  };

  void render() {
    ZoneScoped;
    if (_pending_grid_mesh && _pending_grid_mesh->is_uploaded()) {
      _grid_mesh = std::move(_pending_grid_mesh);
      reset_rt_entity_binning_buffers();
    }
    auto [work_recorder, swapchain_image] = _graphics.record_frame_work();
    auto const framebuffer_extent = swapchain_image->get_extent().eval();
    auto const framebuffer_size = framebuffer_extent.head<2>().eval();
    get_color_render_target(
      work_recorder,
      _albedo_render_target,
      _albedo_render_target_descriptor,
      graphics::Image_format::r16g16b16a16_sfloat,
      framebuffer_extent);
    for (auto i = std::size_t{}; i != 2; ++i) {
      get_color_render_target(
        work_recorder,
        _normal_render_targets[i],
        _normal_render_target_descriptors[i],
        graphics::Image_format::r16g16_snorm,
        framebuffer_extent,
        graphics::Sampler::linear_clamp);
    }
    get_color_render_target(
      work_recorder,
      _motion_vector_render_target,
      _motion_vector_render_target_descriptor,
      graphics::Image_format::r16g16b16a16_sfloat,
      framebuffer_extent);
    get_radiance_render_target(work_recorder, framebuffer_extent);
    get_direct_radiance_render_target(work_recorder, framebuffer_extent);
    get_indirect_radiance_render_targets(work_recorder, framebuffer_extent);
    get_indirect_irradiance_render_targets(work_recorder, framebuffer_extent);
    auto const indirect_rng_state_image_created = get_rng_state_image(
      work_recorder,
      _indirect_rng_state_image,
      _indirect_rng_state_image_descriptor,
      framebuffer_extent);
    auto const direct_rng_state_image_created = get_rng_state_image(
      work_recorder,
      _direct_rng_state_image,
      _direct_rng_state_image_descriptor,
      framebuffer_extent);
    get_color_render_target(
      work_recorder,
      _crosshair_mask_render_target,
      _crosshair_mask_render_target_descriptor,
      graphics::Image_format::r8_unorm,
      framebuffer_extent);
    for (auto i = std::size_t{}; i != 2; ++i) {
      get_depth_render_target(
        work_recorder,
        _depth_render_targets[i],
        _depth_render_target_descriptors[i],
        framebuffer_extent);
    }
    auto const &session = _client.get_session();
    auto const camera =
      session && _local_player && _local_player->player_entity_id &&
          _local_player->humanoid_entity_id
        ? session->get_scene().get_camera(*_local_player->player_entity_id)
        : nullptr;
    auto const sun =
      session && !session->get_scene().empty()
        ? session->get_scene().get_distant_light(scene::elements::sun_light_key)
        : nullptr;
    auto const scene_uniform_offset =
      (_frame_number % max_frames_in_flight) * scene_uniform_data_size;
    if (camera && sun) {
      record_sky_view_pass(
        work_recorder, camera->position, sun->direction, sun->irradiance);
    }
    if (indirect_rng_state_image_created) {
      record_rng_seed_pass(
        work_recorder,
        framebuffer_size,
        _indirect_rng_state_image_descriptor,
        static_cast<u32>(_rng_engine()));
    }
    if (direct_rng_state_image_created) {
      record_rng_seed_pass(
        work_recorder,
        framebuffer_size,
        _direct_rng_state_image_descriptor,
        static_cast<u32>(_rng_engine()));
    }
    record_gbuffer_pass(work_recorder, framebuffer_size, scene_uniform_offset);
    record_rt_entity_binning_pass(work_recorder);
    record_direct_radiance_pass(
      work_recorder, framebuffer_size, scene_uniform_offset);
    record_indirect_radiance_pass(
      work_recorder, framebuffer_size, scene_uniform_offset);
    record_indirect_irradiance_pass(work_recorder, framebuffer_size);
    record_radiance_pass(work_recorder, framebuffer_size);
    record_crosshair_pass(work_recorder, framebuffer_size);
    record_composite_pass(work_recorder, swapchain_image, framebuffer_size);
    _graphics.submit_frame_work(std::move(work_recorder));
  }

  void record_sky_view_pass(
    graphics::Work_recorder &work_recorder,
    math::vec3 camera_position,
    math::vec3 sun_direction,
    math::vec3 sun_irradiance) {
    auto const camera_altitude = camera_position.y();
    work_recorder.bind_compute_pipeline(_sky_view_pipeline);
    work_recorder.push_descriptors(
      0,
      {_transmittance_lut_sampled_descriptor,
       _sky_view_lut_storage_descriptor});
    work_recorder.push_data(4, std::as_bytes(std::span{&camera_altitude, 1}));
    work_recorder.push_data(16, std::as_bytes(std::span{&sun_direction, 1}));
    work_recorder.push_data(32, std::as_bytes(std::span{&sun_irradiance, 1}));
    work_recorder.dispatch(sky_view_lut_size.x(), sky_view_lut_size.y(), 1);
    work_recorder.barrier(
      compute_shader_storage_write_scope,
      compute_shader_sampled_read_scope | fragment_shader_sampled_read_scope);
  }

  void record_gbuffer_pass(
    graphics::Work_recorder &work_recorder,
    math::ivec2 framebuffer_size,
    std::size_t scene_uniform_offset) {
    work_recorder.barrier(
      compute_shader_sampled_read_scope | fragment_shader_sampled_read_scope,
      color_attachment_scope | depth_attachment_scope);
    auto const gbuffer_color_attachments = std::array{
      graphics::Color_attachment_info{.image = _albedo_render_target},
      graphics::Color_attachment_info{
        .image = _normal_render_targets[_frame_number % 2]},
      graphics::Color_attachment_info{.image = _motion_vector_render_target},
    };
    work_recorder.begin_rendering({
      .color_attachments = gbuffer_color_attachments,
      .depth_image = _depth_render_targets[_frame_number % 2],
    });
    work_recorder.set_viewport(framebuffer_size);
    work_recorder.set_scissor(framebuffer_size);
    auto const &session = _client.get_session();
    auto const camera =
      session && _local_player && _local_player->player_entity_id &&
          _local_player->humanoid_entity_id
        ? session->get_scene().get_camera(*_local_player->player_entity_id)
        : nullptr;
    work_recorder.set_depth_test_enabled(true);
    work_recorder.set_depth_write_enabled(true);
    work_recorder.set_depth_compare_op(graphics::Compare_op::greater);
    if (camera) {
      auto const view_matrix =
        (math::x_rotation_matrix(-_local_player->input_state.pitch) *
         math::y_rotation_matrix(-_local_player->input_state.yaw) *
         math::translation_matrix(-camera->position))
          .eval();
      auto const zoom = 1.125f;
      auto const aspect_ratio = static_cast<float>(framebuffer_size.x()) /
                                static_cast<float>(framebuffer_size.y());
      auto const zoom_vec = math::vec2{
        aspect_ratio > 1.0f ? zoom : zoom * aspect_ratio,
        aspect_ratio > 1.0f ? zoom / aspect_ratio : zoom,
      };
      auto const projection_matrix =
        math::perspective_projection_matrix(zoom_vec.x(), zoom_vec.y(), z_near);
      auto const view_projection_matrix =
        (projection_matrix * view_matrix).eval();
      auto const camera_basis =
        (math::translation_matrix(camera->position) *
         math::y_rotation_matrix(_local_player->input_state.yaw) *
         math::x_rotation_matrix(_local_player->input_state.pitch))
          .eval();
      auto const camera_basis_rows = make_rt_matrix_rows(camera_basis);
      auto const scene_uniform_memory = _scene_uniform_buffer->map();
      auto const write_scene_uniform =
        [&]<typename T>(std::size_t offset, T const &value) {
          std::memcpy(
            scene_uniform_memory.get().data() + scene_uniform_offset + offset,
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
      write_scene_uniform(scene_animation_time_offset, _animation_time);
      write_scene_uniform(scene_camera_basis_offset, camera_basis_rows);
      write_scene_uniform(scene_zoom_offset, zoom_vec);
      write_scene_uniform(scene_z_near_offset, z_near);
      auto const sun =
        session->get_scene().get_distant_light(scene::elements::sun_light_key);
      if (sun) {
        write_scene_uniform(scene_sun_direction_offset, sun->direction);
        write_scene_uniform(scene_sun_irradiance_offset, sun->irradiance);
      }
      // draw grid
      if (_grid_mesh && _grid_mesh->is_uploaded()) {
        work_recorder.bind_pipeline(_grid_pipeline);
        work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
        work_recorder.set_cull_mode(graphics::Cull_mode::back);
        work_recorder.bind_index_buffer(
          _grid_mesh->get_index_buffer(), graphics::Index_type::u32);
        work_recorder.push_buffer_reference(
          0, _scene_uniform_buffer, scene_uniform_offset);
        work_recorder.push_buffer_reference(8, _grid_mesh->get_vertex_buffer());
        work_recorder
          .push_buffer_reference(16, _block_texture_registry.get_buffer());
        _block_texture_registry.add_references(work_recorder);
        auto push_normal = [&](math::vec3 const &value) {
          work_recorder.push_data(24, std::as_bytes(std::span{&value, 1}));
        };
        push_normal({1.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, +math::axis3::x);
        push_normal({-1.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, -math::axis3::x);
        push_normal({0.0f, 1.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, +math::axis3::y);
        push_normal({0.0f, -1.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, -math::axis3::y);
        push_normal({0.0f, 0.0f, 1.0f});
        _grid_mesh->record_draws(work_recorder, +math::axis3::z);
        push_normal({0.0f, 0.0f, -1.0f});
        _grid_mesh->record_draws(work_recorder, -math::axis3::z);
      }
      // draw cubes
      work_recorder.bind_pipeline(_mesh_pipeline);
      work_recorder.set_cull_mode(graphics::Cull_mode::back);
      work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
      work_recorder
        .bind_index_buffer(_cube_index_buffer, graphics::Index_type::u16);
      work_recorder
        .push_buffer_reference(0, _scene_uniform_buffer, scene_uniform_offset);
      work_recorder.push_buffer_reference(8, _cube_vertex_buffer);
      for (auto const &box : session->get_scene().get_current_frame().boxes) {
        auto const local_body_key = _local_player->humanoid_entity_id
                                      ? scene::elements::entity_part_key(
                                          *_local_player->humanoid_entity_id, 0)
                                      : 0;
        auto const local_head_key = _local_player->humanoid_entity_id
                                      ? scene::elements::entity_part_key(
                                          *_local_player->humanoid_entity_id, 1)
                                      : 0;
        if (box.key == local_body_key || box.key == local_head_key) {
          continue;
        }
        auto const previous_box =
          session->get_scene().get_previous_box(box.key);
        auto const model_matrix = make_model_matrix(box);
        auto const previous_model_matrix =
          make_model_matrix(previous_box ? *previous_box : box);
        auto const model_rows = Eigen::Matrix<float, 3, 4, Eigen::RowMajor>{
          model_matrix.topRows<3>()};
        auto const previous_model_rows =
          Eigen::Matrix<float, 3, 4, Eigen::RowMajor>{previous_model_matrix
                                                        .topRows<3>()};
        work_recorder.push_data(16, std::as_bytes(std::span{&model_rows, 1}));
        work_recorder
          .push_data(64, std::as_bytes(std::span{&previous_model_rows, 1}));
        work_recorder.draw_indexed({
          .index_count = static_cast<std::uint32_t>(cube_mesh_indices.size()),
          .instance_count = 1,
          .first_index = 0,
          .vertex_offset = 0,
          .first_instance = 0,
        });
      }
    }
    work_recorder.end_rendering();
  }

  // Seeds a freshly created RNG state image; the lighting passes then
  // advance the per-pixel streams in place from frame to frame.
  void record_rng_seed_pass(
    graphics::Work_recorder &work_recorder,
    math::ivec2 framebuffer_size,
    rc::Strong<graphics::Descriptor> const &rng_state_image_descriptor,
    u32 seed) {
    ZoneScoped;
    work_recorder.bind_compute_pipeline(_rng_seed_pipeline);
    work_recorder.push_descriptors(0, {rng_state_image_descriptor});
    work_recorder.push_data(4, std::as_bytes(std::span{&seed, 1}));
    auto const group_count_x = static_cast<u32>((framebuffer_size.x() + 7) / 8);
    auto const group_count_y = static_cast<u32>((framebuffer_size.y() + 7) / 8);
    work_recorder.dispatch(group_count_x, group_count_y, 1);
    work_recorder.barrier(
      compute_shader_storage_write_scope,
      compute_shader_storage_read_scope | compute_shader_storage_write_scope);
  }

  void ensure_rt_entity_capacity(std::size_t capacity) {
    if (_rt_entity_capacity >= capacity) {
      return;
    }
    _rt_entity_capacity = std::max(std::size_t{16}, std::bit_ceil(capacity));
    auto const size = 16 + _rt_entity_capacity * sizeof(Rt_entity);
    for (auto &buffer : _rt_entity_buffers) {
      buffer = _graphics.create_buffer({
        .size = size,
        .usage = graphics::Buffer_usage_flag_bits::shader_device_address,
        .mapping_mode = graphics::Mapping_mode::write_only,
        .min_alignment = 16,
      });
    }
  }

  void reset_rt_entity_binning_buffers() {
    auto const layout =
      make_rt_entity_binning_buffer_layout(_grid_mesh->get_rt_chunk_count());
    for (auto i = std::size_t{}; i != max_frames_in_flight; ++i) {
      _rt_entity_binning_buffers[i] = _graphics.create_buffer({
        .size = layout.size,
        .usage = graphics::Buffer_usage_flag_bits::shader_device_address,
        .mapping_mode = graphics::Mapping_mode::write_only,
        .min_alignment = 8,
      });
    }
  }

  void record_rt_entity_binning_pass(graphics::Work_recorder &work_recorder) {
    auto const &session = _client.get_session();
    if (!session || !_grid_mesh || !_grid_mesh->is_uploaded()) {
      return;
    }
    auto const &boxes = session->get_scene().get_current_frame().boxes;
    ensure_rt_entity_capacity(std::max(boxes.size(), std::size_t{1}));
    auto entities = std::vector<Rt_entity>{};
    entities.reserve(boxes.size());
    for (auto const &box : boxes) {
      auto rotation = math::mat4::Identity().eval();
      rotation.block<3, 3>(0, 0) = box.orientation.toRotationMatrix();
      auto const model =
        (math::translation_matrix(box.position) * rotation).eval();
      auto const inverse_model = model.inverse().eval();
      auto const half_extents = box.half_extents.cwiseAbs().eval();
      entities.push_back({
        .model = make_rt_matrix_rows(model),
        .inverse_model = make_rt_matrix_rows(inverse_model),
        .half_extents =
          math::vec4{
            half_extents.x(), half_extents.y(), half_extents.z(), 0.0f},
        .albedo = math::vec4{0.3f, 0.3f, 0.3f, 0.0f},
      });
    }
    auto const frame = _frame_number % max_frames_in_flight;
    auto const entity_memory = _rt_entity_buffers[frame]->map();
    auto const entity_count = static_cast<std::uint32_t>(entities.size());
    std::memcpy(
      entity_memory.get().data(), &entity_count, sizeof(entity_count));
    if (!entities.empty()) {
      std::memcpy(
        entity_memory.get().data() + 16,
        entities.data(),
        entities.size() * sizeof(Rt_entity));
    }
    auto const layout =
      make_rt_entity_binning_buffer_layout(_grid_mesh->get_rt_chunk_count());
    auto const entity_grid_memory = _rt_entity_binning_buffers[frame]->map();
    std::memset(
      entity_grid_memory.get().data() + layout.nodes_offset,
      0,
      sizeof(std::uint32_t));

    work_recorder.bind_compute_pipeline(_rt_grid_entity_binning_pipeline);
    work_recorder
      .push_buffer_reference(0, _grid_mesh->get_rt_block_grid_buffer());
    work_recorder.push_buffer_reference(8, _rt_entity_buffers[frame]);
    work_recorder.push_buffer_reference(
      16, _rt_entity_binning_buffers[frame], layout.grid_offset);
    work_recorder.push_buffer_reference(
      24, _rt_entity_binning_buffers[frame], layout.nodes_offset);
    work_recorder.dispatch(_grid_mesh->get_rt_chunk_count(), 1, 1);
    work_recorder.barrier(
      compute_shader_storage_write_scope, compute_shader_storage_read_scope);
  }

  void record_direct_radiance_pass(
    graphics::Work_recorder &work_recorder,
    math::ivec2 framebuffer_size,
    std::size_t scene_uniform_offset) {
    ZoneScoped;
    auto const &session = _client.get_session();
    auto const camera =
      session && _local_player && _local_player->player_entity_id &&
          _local_player->humanoid_entity_id
        ? session->get_scene().get_camera(*_local_player->player_entity_id)
        : nullptr;
    if (!camera || !_grid_mesh || !_grid_mesh->is_uploaded()) {
      // Nothing was rasterized into the G-buffer this frame (see
      // record_radiance_pass for why): nothing to light.
      return;
    }
    auto const sun =
      session->get_scene().get_distant_light(scene::elements::sun_light_key);
    if (!sun) {
      return;
    }
    work_recorder.bind_compute_pipeline(_direct_radiance_pipeline);
    work_recorder
      .push_buffer_reference(0, _scene_uniform_buffer, scene_uniform_offset);
    work_recorder.push_descriptors(
      8,
      {_normal_render_target_descriptors[_frame_number % 2],
       _depth_render_target_descriptors[_frame_number % 2],
       _direct_radiance_render_target_storage_descriptor,
       _direct_rng_state_image_descriptor,
       _transmittance_lut_sampled_descriptor});
    auto const frame = _frame_number % max_frames_in_flight;
    auto const layout =
      make_rt_entity_binning_buffer_layout(_grid_mesh->get_rt_chunk_count());
    work_recorder
      .push_buffer_reference(24, _grid_mesh->get_rt_block_grid_buffer());
    work_recorder.push_buffer_reference(32, _rt_entity_buffers[frame]);
    work_recorder.push_buffer_reference(
      40, _rt_entity_binning_buffers[frame], layout.grid_offset);
    work_recorder.push_buffer_reference(
      48, _rt_entity_binning_buffers[frame], layout.nodes_offset);
    auto const group_count_x = static_cast<u32>((framebuffer_size.x() + 7) / 8);
    auto const group_count_y = static_cast<u32>((framebuffer_size.y() + 7) / 8);
    work_recorder.dispatch(group_count_x, group_count_y, 1);
    // No barrier here: this pass is recorded back to back with the indirect
    // radiance pass so the GPU can overlap them, and the indirect pass's
    // trailing barrier covers this pass's storage writes too.
  }

  void record_indirect_radiance_pass(
    graphics::Work_recorder &work_recorder,
    math::ivec2 framebuffer_size,
    std::size_t scene_uniform_offset) {
    ZoneScoped;
    auto const &session = _client.get_session();
    auto const camera =
      session && _local_player && _local_player->player_entity_id &&
          _local_player->humanoid_entity_id
        ? session->get_scene().get_camera(*_local_player->player_entity_id)
        : nullptr;
    if (!camera || !_grid_mesh || !_grid_mesh->is_uploaded()) {
      // Nothing was rasterized into the G-buffer this frame (see
      // record_radiance_pass for why): nothing to trace paths for.
      return;
    }
    auto const sun =
      session->get_scene().get_distant_light(scene::elements::sun_light_key);
    if (!sun) {
      return;
    }
    work_recorder.bind_compute_pipeline(_indirect_radiance_pipeline);
    work_recorder
      .push_buffer_reference(0, _scene_uniform_buffer, scene_uniform_offset);
    work_recorder.push_descriptors(
      8,
      {_normal_render_target_descriptors[_frame_number % 2],
       _depth_render_target_descriptors[_frame_number % 2],
       _sky_view_lut_sampled_descriptor,
       _transmittance_lut_sampled_descriptor,
       _indirect_radiance_render_target_storage_descriptor,
       _indirect_radiance_direction_render_target_storage_descriptor,
       _indirect_rng_state_image_descriptor,
       _blue_noise_texture_descriptors
         [_frame_number % blue_noise_texture_count]});
    auto const frame = _frame_number % max_frames_in_flight;
    auto const layout =
      make_rt_entity_binning_buffer_layout(_grid_mesh->get_rt_chunk_count());
    work_recorder
      .push_buffer_reference(24, _grid_mesh->get_rt_block_grid_buffer());
    work_recorder.push_buffer_reference(32, _rt_entity_buffers[frame]);
    work_recorder.push_buffer_reference(
      40, _rt_entity_binning_buffers[frame], layout.grid_offset);
    work_recorder.push_buffer_reference(
      48, _rt_entity_binning_buffers[frame], layout.nodes_offset);
    auto const group_count_x = static_cast<u32>((framebuffer_size.x() + 7) / 8);
    auto const group_count_y = static_cast<u32>((framebuffer_size.y() + 7) / 8);
    work_recorder.dispatch(group_count_x, group_count_y, 1);
    // Also covers the direct radiance pass recorded just before this one.
    // The storage read/write scope covers next frame's lighting passes
    // reading and updating the RNG state images.
    work_recorder.barrier(
      compute_shader_storage_write_scope,
      compute_shader_sampled_read_scope | compute_shader_storage_read_scope |
        compute_shader_storage_write_scope);
  }

  void record_indirect_irradiance_pass(
    graphics::Work_recorder &work_recorder, math::ivec2 framebuffer_size) {
    ZoneScoped;
    auto const &session = _client.get_session();
    auto const camera =
      session && _local_player && _local_player->player_entity_id &&
          _local_player->humanoid_entity_id
        ? session->get_scene().get_camera(*_local_player->player_entity_id)
        : nullptr;
    if (!camera || !_grid_mesh || !_grid_mesh->is_uploaded()) {
      // No indirect radiance samples were traced this frame (see
      // record_radiance_pass for why): nothing to compute irradiance for.
      return;
    }
    work_recorder.bind_compute_pipeline(_indirect_irradiance_pipeline);
    work_recorder.push_descriptors(
      0,
      {_depth_render_target_descriptors[_frame_number % 2],
       _depth_render_target_descriptors[(_frame_number + 1) % 2],
       _normal_render_target_descriptors[_frame_number % 2],
       _normal_render_target_descriptors[(_frame_number + 1) % 2],
       _indirect_radiance_render_target_descriptor,
       _indirect_radiance_direction_render_target_descriptor,
       _indirect_irradiance_render_target_storage_descriptors
         [_frame_number % 2],
       _indirect_irradiance_render_target_descriptors[(_frame_number + 1) % 2],
       _motion_vector_render_target_descriptor});
    // History is only valid if last frame's pass wrote the other image.
    auto const history_valid = u32{
      _last_indirect_irradiance_frame &&
      *_last_indirect_irradiance_frame + 1 == _frame_number};
    work_recorder.push_data(20, std::as_bytes(std::span{&history_valid, 1}));
    work_recorder.push_data(24, std::as_bytes(std::span{&z_near, 1}));
    auto constexpr zoom = 1.125f;
    auto const aspect_ratio = static_cast<f32>(framebuffer_size.x()) /
                              static_cast<f32>(framebuffer_size.y());
    auto const zoom_vec = math::vec2{
      aspect_ratio > 1.0f ? zoom : zoom * aspect_ratio,
      aspect_ratio > 1.0f ? zoom / aspect_ratio : zoom,
    };
    auto const camera_basis =
      (math::translation_matrix(camera->position) *
       math::y_rotation_matrix(_local_player->input_state.yaw) *
       math::x_rotation_matrix(_local_player->input_state.pitch))
        .eval();
    auto const camera_basis_rows =
      Eigen::Matrix<float, 3, 4, Eigen::RowMajor>{camera_basis.topRows<3>()};
    work_recorder
      .push_data(32, std::as_bytes(std::span{&camera_basis_rows, 1}));
    work_recorder.push_data(80, std::as_bytes(std::span{&zoom_vec, 1}));
    _last_indirect_irradiance_frame = _frame_number;
    auto const group_count_x = static_cast<u32>((framebuffer_size.x() + 7) / 8);
    auto const group_count_y = static_cast<u32>((framebuffer_size.y() + 7) / 8);
    work_recorder.dispatch(group_count_x, group_count_y, 1);
    work_recorder.barrier(
      compute_shader_storage_write_scope, compute_shader_sampled_read_scope);
  }

  void record_radiance_pass(
    graphics::Work_recorder &work_recorder, math::ivec2 framebuffer_size) {
    ZoneScoped;
    auto const &session = _client.get_session();
    auto const camera =
      session && _local_player && _local_player->player_entity_id &&
          _local_player->humanoid_entity_id
        ? session->get_scene().get_camera(*_local_player->player_entity_id)
        : nullptr;
    if (!camera || !_grid_mesh || !_grid_mesh->is_uploaded()) {
      // No camera yet, or the grid mesh (and its shadow voxel buffer the
      // shadow raymarch depends on) hasn't finished uploading: nothing was
      // rasterized into the G-buffer this frame, so just clear the radiance
      // target directly instead of dispatching a lighting pass with no
      // valid view or grid to light with.
      work_recorder
        .barrier(compute_shader_storage_write_scope, color_attachment_scope);
      auto const radiance_color_attachments = std::array{
        graphics::Color_attachment_info{
          .image = _radiance_render_target, .clear_value = sky_color},
      };
      work_recorder.begin_rendering({
        .color_attachments = radiance_color_attachments,
      });
      work_recorder.end_rendering();
      work_recorder
        .barrier(color_attachment_scope, fragment_shader_sampled_read_scope);
      return;
    }
    auto constexpr zoom = 1.125f;
    auto const aspect_ratio = static_cast<f32>(framebuffer_size.x()) /
                              static_cast<f32>(framebuffer_size.y());
    auto const zoom_vec = math::vec2{
      aspect_ratio > 1.0f ? zoom : zoom * aspect_ratio,
      aspect_ratio > 1.0f ? zoom / aspect_ratio : zoom,
    };
    auto const camera_basis =
      (math::translation_matrix(camera->position) *
       math::y_rotation_matrix(_local_player->input_state.yaw) *
       math::x_rotation_matrix(_local_player->input_state.pitch))
        .eval();
    // Rows contiguous, matching the row_major mat4x3 push constants.
    auto const camera_basis_rows =
      Eigen::Matrix<float, 3, 4, Eigen::RowMajor>{camera_basis.topRows<3>()};
    work_recorder.bind_compute_pipeline(_radiance_pipeline);
    work_recorder.push_data(0, std::as_bytes(std::span{&camera_basis_rows, 1}));
    work_recorder.push_data(48, std::as_bytes(std::span{&zoom_vec, 1}));
    work_recorder.push_descriptors(
      56,
      {_albedo_render_target_descriptor,
       _depth_render_target_descriptors[_frame_number % 2],
       _sky_view_lut_sampled_descriptor,
       _radiance_render_target_storage_descriptor,
       _direct_radiance_render_target_descriptor,
       _indirect_irradiance_render_target_descriptors[_frame_number % 2]});
    auto const group_count_x = static_cast<u32>((framebuffer_size.x() + 7) / 8);
    auto const group_count_y = static_cast<u32>((framebuffer_size.y() + 7) / 8);
    work_recorder.dispatch(group_count_x, group_count_y, 1);
    work_recorder.barrier(
      compute_shader_storage_write_scope, fragment_shader_sampled_read_scope);
  }

  void record_crosshair_pass(
    graphics::Work_recorder &work_recorder, math::ivec2 framebuffer_size) {
    auto const color_attachments = std::array{
      graphics::Color_attachment_info{.image = _crosshair_mask_render_target},
    };
    work_recorder.begin_rendering({
      .color_attachments = color_attachments,
    });
    work_recorder.set_viewport(framebuffer_size);
    work_recorder.set_scissor(framebuffer_size);
    work_recorder.bind_pipeline(_crosshair_pipeline);
    work_recorder.set_cull_mode(graphics::Cull_mode::none);
    work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
    work_recorder
      .bind_index_buffer(_crosshair_index_buffer, graphics::Index_type::u16);
    work_recorder.push_data(0, std::as_bytes(std::span{&framebuffer_size, 1}));
    work_recorder.draw_indexed({
      .index_count = static_cast<std::uint32_t>(crosshair_indices.size()),
      .instance_count = 1,
      .first_index = 0,
      .vertex_offset = 0,
      .first_instance = 0,
    });
    work_recorder.end_rendering();
  }

  void record_composite_pass(
    graphics::Work_recorder &work_recorder,
    rc::Strong<graphics::Image> const &swapchain_image,
    math::ivec2 framebuffer_size) {
    work_recorder.barrier(
      color_attachment_scope | depth_attachment_scope,
      fragment_shader_sampled_read_scope);
    work_recorder.transition_image_layout(
      {},
      color_attachment_scope,
      graphics::Image_layout::undefined,
      graphics::Image_layout::general,
      swapchain_image);
    auto const color_attachments = std::array{
      graphics::Color_attachment_info{.image = swapchain_image},
    };
    work_recorder.begin_rendering({
      .color_attachments = color_attachments,
    });
    work_recorder.set_viewport(framebuffer_size);
    work_recorder.set_scissor(framebuffer_size);
    work_recorder
      .bind_pipeline(get_composite_pipeline(swapchain_image->get_format()));
    work_recorder.set_cull_mode(graphics::Cull_mode::none);
    work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
    work_recorder
      .bind_index_buffer(_composite_index_buffer, graphics::Index_type::u16);
    work_recorder.push_descriptors(
      0,
      {_radiance_render_target_descriptor,
       _crosshair_mask_render_target_descriptor});
    work_recorder.push_data(4, std::as_bytes(std::span{&_frame_number, 1}));
    work_recorder.draw_indexed({
      .index_count = static_cast<u32>(composite_indices.size()),
      .instance_count = 1,
      .first_index = 0,
      .vertex_offset = 0,
      .first_instance = 0,
    });
    work_recorder.end_rendering();
  }

  void on_key(
    glfw::Window, glfw::Key key, int, glfw::Press_action action, int) override {
    if (key == glfw::Key::k_escape && action == glfw::Press_action::press) {
      _glfw_window->set_cursor_input_mode(glfw::Cursor_input_mode::normal);
    } else if (_local_player) {
      auto input_state = _local_player->input_state;
      switch (key) {
      case glfw::Key::k_e:
        input_state.move_forward = action != glfw::Press_action::release;
        break;
      case glfw::Key::k_s:
        input_state.move_left = action != glfw::Press_action::release;
        break;
      case glfw::Key::k_d:
        input_state.move_backward = action != glfw::Press_action::release;
        break;
      case glfw::Key::k_f:
        input_state.move_right = action != glfw::Press_action::release;
        break;
      case glfw::Key::k_backspace:
        input_state.jump = action != glfw::Press_action::release;
        break;
      case glfw::Key::k_left_shift:
      case glfw::Key::k_right_shift:
        input_state.run = action != glfw::Press_action::release;
        break;
      case glfw::Key::k_g:
        input_state.drop = action != glfw::Press_action::release;
        break;
      case glfw::Key::k_1:
        input_state.slot_index = static_cast<u8>(game::Block::stone);
        break;
      case glfw::Key::k_2:
        input_state.slot_index = static_cast<u8>(game::Block::conveyor);
        break;
      case glfw::Key::k_3:
        input_state.slot_index = static_cast<u8>(game::Block::light);
        break;
      default:
      }
      _local_player->input_state = input_state;
    }
  }

  void on_mouse_button(
    glfw::Window,
    glfw::Mouse_button button,
    glfw::Press_state action,
    int) override {
    if (
      _glfw_window->get_cursor_input_mode() ==
        glfw::Cursor_input_mode::disabled &&
      _local_player) {
      auto input_state = _local_player->input_state;
      if (button == glfw::Mouse_button::mb_left) {
        input_state.use_primary = action != glfw::Press_state::release;
      } else if (button == glfw::Mouse_button::mb_right) {
        input_state.use_secondary = action != glfw::Press_state::release;
      }
      _local_player->input_state = input_state;
    }
    if (
      button == glfw::Mouse_button::mb_right &&
      action == glfw::Press_state::press) {
      _glfw_window->set_cursor_input_mode(glfw::Cursor_input_mode::disabled);
    }
  }

  void on_cursor_pos(
    glfw::Window, double, double, double dxpos, double dypos) override {
    if (
      _local_player && _glfw_window->get_cursor_input_mode() ==
                         glfw::Cursor_input_mode::disabled) {
      auto input_state = _local_player->input_state;
      input_state.yaw -=
        static_cast<float>(dxpos * constants::mouselook_sensititvity);
      input_state.pitch +=
        static_cast<float>(dypos * constants::mouselook_sensititvity);
      input_state.pitch = std::clamp(
        input_state.pitch,
        -0.5f * std::numbers::pi_v<float>,
        0.5f * std::numbers::pi_v<float>);
      _local_player->input_state = input_state;
    }
  }

  void update_grid_mesh() {
    ZoneScoped;
    auto &session = _client.get_session();
    if (!session) {
      _grid_mesh.reset();
      _pending_grid_mesh.reset();
      return;
    }
    auto &scene = session->get_scene();
    if (!scene.get_grid_remesh_flag()) {
      return;
    }
    _pending_grid_mesh = std::make_unique<Grid_mesh>(Grid_mesh_create_info{
      .graphics = &_graphics,
      .grid = &scene.get_grid(),
      .block_model_registry = &_block_model_registry,
    });
    scene.reset_grid_remesh_flag();
  }

  void init_transmittance_lut() {
    auto transmittance_shader = graphics::load_shader(
      "./assets/shaders/atmosphere/transmittance.comp.spv");
    auto transmittance_pipeline =
      _graphics.create_compute_pipeline({.shader = &transmittance_shader});
    _transmittance_lut = _graphics.create_image({
      .dimensionality = 2,
      .format = graphics::Image_format::r16g16b16a16_sfloat,
      .extent = {transmittance_lut_size.x(), transmittance_lut_size.y(), 1},
      .mip_level_count = 1,
      .array_layer_count = 1,
      .usage = graphics::Image_usage_flag_bits::sampled |
               graphics::Image_usage_flag_bits::storage,
    });
    _transmittance_lut_sampled_descriptor =
      _graphics.create_sampled_image_descriptor(
        _transmittance_lut, graphics::Sampler::linear_clamp);
    auto const transmittance_lut_storage_descriptor =
      _graphics.create_storage_image_descriptor(_transmittance_lut);
    auto work_recorder = _graphics.record_transient_work();
    work_recorder.transition_image_layout(
      {},
      compute_shader_storage_write_scope,
      graphics::Image_layout::undefined,
      graphics::Image_layout::general,
      _transmittance_lut);
    work_recorder.bind_compute_pipeline(transmittance_pipeline);
    work_recorder.push_descriptors(0, {transmittance_lut_storage_descriptor});
    work_recorder
      .dispatch(transmittance_lut_size.x(), transmittance_lut_size.y(), 1);
    work_recorder.barrier(
      compute_shader_storage_write_scope, fragment_shader_sampled_read_scope);
    auto work = _graphics.submit_transient_work(std::move(work_recorder));
    work->await();
  }

  void init_sky_view_lut() {
    _sky_view_lut = _graphics.create_image({
      .dimensionality = 2,
      .format = graphics::Image_format::r16g16b16a16_sfloat,
      .extent = {sky_view_lut_size.x(), sky_view_lut_size.y(), 1},
      .mip_level_count = 1,
      .array_layer_count = 1,
      .usage = graphics::Image_usage_flag_bits::sampled |
               graphics::Image_usage_flag_bits::storage,
    });
    _sky_view_lut_sampled_descriptor =
      _graphics.create_sampled_image_descriptor(
        _sky_view_lut, graphics::Sampler::lat_long);
    _sky_view_lut_storage_descriptor =
      _graphics.create_storage_image_descriptor(_sky_view_lut);
    auto work_recorder = _graphics.record_transient_work();
    work_recorder.transition_image_layout(
      {},
      compute_shader_storage_write_scope,
      graphics::Image_layout::undefined,
      graphics::Image_layout::general,
      _sky_view_lut);
    auto work = _graphics.submit_transient_work(std::move(work_recorder));
    work->await();
  }

  void get_color_render_target(
    graphics::Work_recorder &work_recorder,
    rc::Strong<graphics::Image> &image,
    rc::Strong<graphics::Descriptor> &descriptor,
    graphics::Image_format format,
    math::ivec3 extent,
    graphics::Sampler sampler = graphics::Sampler::nearest) {
    auto const create_image = !image || image->get_extent() != extent;
    if (create_image) {
      image = _graphics.create_image({
        .dimensionality = 2,
        .format = format,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::sampled |
                 graphics::Image_usage_flag_bits::color_attachment,
      });
      work_recorder.transition_image_layout(
        {},
        color_attachment_scope,
        graphics::Image_layout::undefined,
        graphics::Image_layout::general,
        image);
      descriptor = _graphics.create_sampled_image_descriptor(image, sampler);
    }
  }

  void get_depth_render_target(
    graphics::Work_recorder &work_recorder,
    rc::Strong<graphics::Image> &image,
    rc::Strong<graphics::Descriptor> &descriptor,
    math::ivec3 extent) {
    auto const create_image = !image || image->get_extent() != extent;
    if (create_image) {
      image = _graphics.create_image({
        .dimensionality = 2,
        .format = graphics::Image_format::d32_sfloat,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::sampled |
                 graphics::Image_usage_flag_bits::depth_attachment,
      });
      work_recorder.transition_image_layout(
        {},
        depth_attachment_scope,
        graphics::Image_layout::undefined,
        graphics::Image_layout::general,
        image);
      descriptor = _graphics.create_sampled_image_descriptor(
        image, graphics::Sampler::linear_clamp);
    }
  }

  void get_radiance_render_target(
    graphics::Work_recorder &work_recorder, math::ivec3 extent) {
    auto const create_image = !_radiance_render_target ||
                              _radiance_render_target->get_extent() != extent;
    if (create_image) {
      _radiance_render_target = _graphics.create_image({
        .dimensionality = 2,
        .format = graphics::Image_format::r16g16b16a16_sfloat,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::sampled |
                 graphics::Image_usage_flag_bits::storage |
                 graphics::Image_usage_flag_bits::color_attachment,
      });
      work_recorder.transition_image_layout(
        {},
        compute_shader_storage_write_scope,
        graphics::Image_layout::undefined,
        graphics::Image_layout::general,
        _radiance_render_target);
      _radiance_render_target_descriptor =
        _graphics.create_sampled_image_descriptor(_radiance_render_target);
      _radiance_render_target_storage_descriptor =
        _graphics.create_storage_image_descriptor(_radiance_render_target);
    }
  }

  void get_direct_radiance_render_target(
    graphics::Work_recorder &work_recorder, math::ivec3 extent) {
    auto const create_image =
      !_direct_radiance_render_target ||
      _direct_radiance_render_target->get_extent() != extent;
    if (create_image) {
      _direct_radiance_render_target = _graphics.create_image({
        .dimensionality = 2,
        .format = graphics::Image_format::r16g16b16a16_sfloat,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::sampled |
                 graphics::Image_usage_flag_bits::storage,
      });
      work_recorder.transition_image_layout(
        {},
        compute_shader_storage_write_scope,
        graphics::Image_layout::undefined,
        graphics::Image_layout::general,
        _direct_radiance_render_target);
      _direct_radiance_render_target_descriptor =
        _graphics
          .create_sampled_image_descriptor(_direct_radiance_render_target);
      _direct_radiance_render_target_storage_descriptor =
        _graphics
          .create_storage_image_descriptor(_direct_radiance_render_target);
    }
  }

  void get_indirect_radiance_render_targets(
    graphics::Work_recorder &work_recorder, math::ivec3 extent) {
    auto const create_images =
      !_indirect_radiance_render_target ||
      _indirect_radiance_render_target->get_extent() != extent;
    if (create_images) {
      _indirect_radiance_render_target = _graphics.create_image({
        .dimensionality = 2,
        .format = graphics::Image_format::r16g16b16a16_sfloat,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::sampled |
                 graphics::Image_usage_flag_bits::storage,
      });
      _indirect_radiance_direction_render_target = _graphics.create_image({
        .dimensionality = 2,
        .format = graphics::Image_format::r16g16b16a16_snorm,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::sampled |
                 graphics::Image_usage_flag_bits::storage,
      });
      work_recorder.transition_image_layout(
        {},
        compute_shader_storage_write_scope,
        graphics::Image_layout::undefined,
        graphics::Image_layout::general,
        _indirect_radiance_render_target);
      work_recorder.transition_image_layout(
        {},
        compute_shader_storage_write_scope,
        graphics::Image_layout::undefined,
        graphics::Image_layout::general,
        _indirect_radiance_direction_render_target);
      _indirect_radiance_render_target_descriptor =
        _graphics
          .create_sampled_image_descriptor(_indirect_radiance_render_target);
      _indirect_radiance_render_target_storage_descriptor =
        _graphics
          .create_storage_image_descriptor(_indirect_radiance_render_target);
      _indirect_radiance_direction_render_target_descriptor =
        _graphics.create_sampled_image_descriptor(
          _indirect_radiance_direction_render_target);
      _indirect_radiance_direction_render_target_storage_descriptor =
        _graphics.create_storage_image_descriptor(
          _indirect_radiance_direction_render_target);
    }
  }

  void get_indirect_irradiance_render_targets(
    graphics::Work_recorder &work_recorder, math::ivec3 extent) {
    auto const create_images =
      !_indirect_irradiance_render_targets[0] ||
      _indirect_irradiance_render_targets[0]->get_extent() != extent;
    if (create_images) {
      _last_indirect_irradiance_frame.reset();
      for (auto i = std::size_t{}; i != 2; ++i) {
        _indirect_irradiance_render_targets[i] = _graphics.create_image({
          .dimensionality = 2,
          .format = graphics::Image_format::r16g16b16a16_sfloat,
          .extent = extent,
          .mip_level_count = 1,
          .array_layer_count = 1,
          .usage = graphics::Image_usage_flag_bits::sampled |
                   graphics::Image_usage_flag_bits::storage,
        });
        work_recorder.transition_image_layout(
          {},
          compute_shader_storage_write_scope,
          graphics::Image_layout::undefined,
          graphics::Image_layout::general,
          _indirect_irradiance_render_targets[i]);
        _indirect_irradiance_render_target_descriptors[i] =
          _graphics.create_sampled_image_descriptor(
            _indirect_irradiance_render_targets[i],
            graphics::Sampler::linear_clamp);
        _indirect_irradiance_render_target_storage_descriptors[i] =
          _graphics.create_storage_image_descriptor(
            _indirect_irradiance_render_targets[i]);
      }
    }
  }

  // Returns true if the image was (re)created and needs seeding.
  bool get_rng_state_image(
    graphics::Work_recorder &work_recorder,
    rc::Strong<graphics::Image> &image,
    rc::Strong<graphics::Descriptor> &descriptor,
    math::ivec3 extent) {
    auto const create_image = !image || image->get_extent() != extent;
    if (create_image) {
      image = _graphics.create_image({
        .dimensionality = 2,
        .format = graphics::Image_format::r32_uint,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::storage,
      });
      work_recorder.transition_image_layout(
        {},
        compute_shader_storage_write_scope,
        graphics::Image_layout::undefined,
        graphics::Image_layout::general,
        image);
      descriptor = _graphics.create_storage_image_descriptor(image);
    }
    return create_image;
  }

  rc::Strong<graphics::Buffer>
  upload_vertices(std::span<std::byte const> data) {
    auto const staging_buffer = _graphics.create_staging_buffer(data);
    auto vertex_buffer = _graphics.create_buffer({
      .size = data.size(),
      .usage = graphics::Buffer_usage_flag_bits::transfer_dst |
               graphics::Buffer_usage_flag_bits::shader_device_address,
    });
    auto work_recorder = _graphics.record_transient_work();
    work_recorder.copy_buffer(
      staging_buffer,
      vertex_buffer,
      {
        .src_offset = 0,
        .dst_offset = 0,
        .size = data.size(),
      });
    work_recorder.barrier(
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
        .access_mask = graphics::Access_flag_bits::transfer_write,
      },
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::vertex_shader,
        .access_mask = graphics::Access_flag_bits::shader_storage_read,
      });
    auto work = _graphics.submit_transient_work(std::move(work_recorder));
    work->await();
    return vertex_buffer;
  }

  rc::Strong<graphics::Buffer> upload_indices(std::span<std::byte const> data) {
    auto const staging_buffer = _graphics.create_staging_buffer(data);
    auto index_buffer = _graphics.create_index_buffer(data.size());
    auto work_recorder = _graphics.record_transient_work();
    work_recorder.copy_buffer(
      staging_buffer,
      index_buffer,
      {
        .src_offset = 0,
        .dst_offset = 0,
        .size = data.size(),
      });
    work_recorder.barrier(
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
        .access_mask = graphics::Access_flag_bits::transfer_write,
      },
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::index_input,
        .access_mask = graphics::Access_flag_bits::index_read,
      });
    auto work = _graphics.submit_transient_work(std::move(work_recorder));
    work->await();
    return index_buffer;
  }

  rc::Strong<graphics::Image> upload_texture(char const *path) {
    auto const file = load_file(path);
    auto const ppm_image = ppm::load_ppm(file);
    auto pixels = std::vector<std::byte>(
      static_cast<std::size_t>(ppm_image.width) *
      static_cast<std::size_t>(ppm_image.height) * 4);
    for (auto i = std::size_t{};
         i != static_cast<std::size_t>(ppm_image.width) *
                static_cast<std::size_t>(ppm_image.height);
         ++i) {
      pixels[i * 4 + 0] = ppm_image.data[i * 3 + 2];
      pixels[i * 4 + 1] = ppm_image.data[i * 3 + 1];
      pixels[i * 4 + 2] = ppm_image.data[i * 3 + 0];
      pixels[i * 4 + 3] = static_cast<std::byte>(0xff);
    }
    auto image = _graphics.create_image({
      .dimensionality = 2,
      .format = graphics::Image_format::b8g8r8a8_srgb,
      .extent = {ppm_image.width, ppm_image.height, 1},
      .mip_level_count = 1,
      .array_layer_count = 1,
      .usage = graphics::Image_usage_flag_bits::sampled |
               graphics::Image_usage_flag_bits::transfer_dst,
    });
    auto const staging_buffer = _graphics.create_staging_buffer(pixels);
    auto work_recorder = _graphics.record_transient_work();
    work_recorder.transition_image_layout(
      {},
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
        .access_mask = graphics::Access_flag_bits::transfer_write,
      },
      graphics::Image_layout::undefined,
      graphics::Image_layout::general,
      image);
    work_recorder.copy_buffer_to_image(
      staging_buffer,
      image,
      {
        .src_offset = 0,
        .dst_mip_level = 0,
        .dst_base_array_layer = 0,
        .dst_array_layer_count = 1,
        .dst_offset = {0, 0, 0},
        .dst_extent = {ppm_image.width, ppm_image.height, 1},
      });
    work_recorder.barrier(
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
        .access_mask = graphics::Access_flag_bits::transfer_write,
      },
      {
        .stage_mask = graphics::Pipeline_stage_flag_bits::fragment_shader,
        .access_mask = graphics::Access_flag_bits::shader_sampled_read,
      });
    auto work = _graphics.submit_transient_work(std::move(work_recorder));
    work->await();
    return image;
  }

  rc::Strong<graphics::Pipeline>
  get_composite_pipeline(graphics::Image_format swapchain_image_format) {
    if (
      !_composite_pipeline_color_format ||
      *_composite_pipeline_color_format != swapchain_image_format) {
      _composite_pipeline = make_composite_pipeline(swapchain_image_format);
      _composite_pipeline_color_format = swapchain_image_format;
    }
    return {_composite_pipeline};
  }

  rc::Strong<graphics::Pipeline> make_grid_pipeline() {
    auto const shader_stages =
      std::vector<graphics::Pipeline_shader_stage_create_info>{
        {
          .stage = graphics::Shader_stage_flag_bits::vertex,
          .shader = &_grid_vertex_shader,
        },
        {
          .stage = graphics::Shader_stage_flag_bits::fragment,
          .shader = &_grid_fragment_shader,
        },
      };
    auto const color_attachment_formats = std::array{
      graphics::Image_format::r16g16b16a16_sfloat,
      graphics::Image_format::r16g16_snorm,
      graphics::Image_format::r16g16b16a16_sfloat,
    };
    auto pipeline = _graphics.create_pipeline({
      .shader_stages = std::span{shader_stages},
      .input_assembly_state =
        {
          .primitive_topology = graphics::Primitive_topology::triangle_list,
        },
      .depth_state =
        {
          .depth_attachment_enabled = true,
        },
      .color_state =
        {
          .color_attachment_formats = color_attachment_formats,
        },
    });
    return pipeline;
  }

  rc::Strong<graphics::Pipeline> make_mesh_pipeline() {
    auto const shader_stages =
      std::vector<graphics::Pipeline_shader_stage_create_info>{
        {
          .stage = graphics::Shader_stage_flag_bits::vertex,
          .shader = &_mesh_vertex_shader,
        },
        {
          .stage = graphics::Shader_stage_flag_bits::fragment,
          .shader = &_mesh_fragment_shader,
        },
      };
    auto const color_attachment_formats = std::array{
      graphics::Image_format::r16g16b16a16_sfloat,
      graphics::Image_format::r16g16_snorm,
      graphics::Image_format::r16g16b16a16_sfloat,
    };
    auto pipeline = _graphics.create_pipeline({
      .shader_stages = std::span{shader_stages},
      .input_assembly_state =
        {
          .primitive_topology = graphics::Primitive_topology::triangle_list,
        },
      .depth_state =
        {
          .depth_attachment_enabled = true,
        },
      .color_state =
        {
          .color_attachment_formats = color_attachment_formats,
        },
    });
    return pipeline;
  }

  rc::Strong<graphics::Pipeline> make_crosshair_pipeline() {
    auto const shader_stages =
      std::vector<graphics::Pipeline_shader_stage_create_info>{
        {
          .stage = graphics::Shader_stage_flag_bits::vertex,
          .shader = &_crosshair_vertex_shader,
        },
        {
          .stage = graphics::Shader_stage_flag_bits::fragment,
          .shader = &_crosshair_fragment_shader,
        },
      };
    auto const color_attachment_format = graphics::Image_format::r8_unorm;
    auto pipeline = _graphics.create_pipeline({
      .shader_stages = std::span{shader_stages},
      .input_assembly_state =
        {
          .primitive_topology = graphics::Primitive_topology::triangle_list,
        },
      .depth_state =
        {
          .depth_attachment_enabled = false,
        },
      .color_state =
        {
          .color_attachment_formats = {&color_attachment_format, 1},
        },
    });
    return pipeline;
  }

  rc::Strong<graphics::Pipeline>
  make_composite_pipeline(graphics::Image_format swapchain_image_format) {
    auto const shader_stages =
      std::vector<graphics::Pipeline_shader_stage_create_info>{
        {
          .stage = graphics::Shader_stage_flag_bits::vertex,
          .shader = &_composite_vertex_shader,
        },
        {
          .stage = graphics::Shader_stage_flag_bits::fragment,
          .shader = &_composite_fragment_shader,
        },
      };
    auto const color_attachment_format = swapchain_image_format;
    auto pipeline = _graphics.create_pipeline({
      .shader_stages = std::span{shader_stages},
      .input_assembly_state =
        {
          .primitive_topology = graphics::Primitive_topology::triangle_list,
        },
      .depth_state =
        {
          .depth_attachment_enabled = false,
        },
      .color_state =
        {
          .color_attachment_formats = {&color_attachment_format, 1},
        },
    });
    return pipeline;
  }

  static auto constexpr fragment_shader_sampled_read_scope =
    graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::fragment_shader,
      .access_mask = graphics::Access_flag_bits::shader_sampled_read,
    };

  static auto constexpr compute_shader_sampled_read_scope =
    graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
      .access_mask = graphics::Access_flag_bits::shader_sampled_read,
    };

  static auto constexpr compute_shader_storage_write_scope =
    graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
      .access_mask = graphics::Access_flag_bits::shader_storage_write,
    };

  static auto constexpr compute_shader_storage_read_scope =
    graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
      .access_mask = graphics::Access_flag_bits::shader_storage_read,
    };

  static auto constexpr transfer_write_scope = graphics::Synchronization_scope{
    .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
    .access_mask = graphics::Access_flag_bits::transfer_write,
  };

  static auto constexpr color_attachment_scope =
    graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::color_attachment_output,
      .access_mask = graphics::Access_flag_bits::color_attachment_write,
    };

  static auto constexpr depth_attachment_scope =
    graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::early_fragment_tests |
                    graphics::Pipeline_stage_flag_bits::late_fragment_tests,
      .access_mask = graphics::Access_flag_bits::depth_stencil_attachment_read |
                     graphics::Access_flag_bits::depth_stencil_attachment_write,
    };

  State _state{State::initial};
  Client _client;
  Local_player *_local_player{};
  enet::Address _server_address;
  glfw::Unique_window _glfw_window{};
  vk::UniqueSurfaceKHR _vk_surface{};
  graphics::Graphics _graphics{};
  // Ping-pong pair: [_frame_number % 2] is written this frame, the other
  // holds last frame's depth for temporal reprojection.
  std::array<rc::Strong<graphics::Image>, 2> _depth_render_targets{};
  std::array<rc::Strong<graphics::Descriptor>, 2>
    _depth_render_target_descriptors{};
  rc::Strong<graphics::Image> _albedo_render_target{};
  rc::Strong<graphics::Descriptor> _albedo_render_target_descriptor{};
  // Ping-pong pair: [_frame_number % 2] is written this frame, the other
  // holds last frame's normals for temporal reprojection.
  std::array<rc::Strong<graphics::Image>, 2> _normal_render_targets{};
  std::array<rc::Strong<graphics::Descriptor>, 2>
    _normal_render_target_descriptors{};
  rc::Strong<graphics::Image> _motion_vector_render_target{};
  rc::Strong<graphics::Descriptor> _motion_vector_render_target_descriptor{};
  rc::Strong<graphics::Image> _radiance_render_target{};
  rc::Strong<graphics::Descriptor> _radiance_render_target_descriptor{};
  rc::Strong<graphics::Descriptor> _radiance_render_target_storage_descriptor{};
  rc::Strong<graphics::Image> _direct_radiance_render_target{};
  rc::Strong<graphics::Descriptor> _direct_radiance_render_target_descriptor{};
  rc::Strong<graphics::Descriptor>
    _direct_radiance_render_target_storage_descriptor{};
  rc::Strong<graphics::Image> _indirect_radiance_render_target{};
  rc::Strong<graphics::Descriptor>
    _indirect_radiance_render_target_descriptor{};
  rc::Strong<graphics::Descriptor>
    _indirect_radiance_render_target_storage_descriptor{};
  rc::Strong<graphics::Image> _indirect_radiance_direction_render_target{};
  rc::Strong<graphics::Descriptor>
    _indirect_radiance_direction_render_target_descriptor{};
  rc::Strong<graphics::Descriptor>
    _indirect_radiance_direction_render_target_storage_descriptor{};
  // Ping-pong pair: [_frame_number % 2] is written this frame, the other
  // holds last frame's irradiance.
  std::array<rc::Strong<graphics::Image>, 2>
    _indirect_irradiance_render_targets{};
  std::array<rc::Strong<graphics::Descriptor>, 2>
    _indirect_irradiance_render_target_descriptors{};
  std::array<rc::Strong<graphics::Descriptor>, 2>
    _indirect_irradiance_render_target_storage_descriptors{};
  std::optional<u32> _last_indirect_irradiance_frame{};
  rc::Strong<graphics::Image> _indirect_rng_state_image{};
  rc::Strong<graphics::Descriptor> _indirect_rng_state_image_descriptor{};
  rc::Strong<graphics::Image> _direct_rng_state_image{};
  rc::Strong<graphics::Descriptor> _direct_rng_state_image_descriptor{};
  std::array<rc::Strong<graphics::Descriptor>, blue_noise_texture_count>
    _blue_noise_texture_descriptors{};
  std::mt19937 _rng_engine{std::random_device{}()};
  rc::Strong<graphics::Image> _crosshair_mask_render_target{};
  rc::Strong<graphics::Descriptor> _crosshair_mask_render_target_descriptor{};
  graphics::Shader _grid_vertex_shader;
  graphics::Shader _grid_fragment_shader;
  graphics::Shader _mesh_vertex_shader;
  graphics::Shader _mesh_fragment_shader;
  graphics::Shader _crosshair_vertex_shader;
  graphics::Shader _crosshair_fragment_shader;
  graphics::Shader _composite_vertex_shader;
  graphics::Shader _composite_fragment_shader;
  graphics::Shader _sky_view_compute_shader;
  graphics::Shader _direct_radiance_compute_shader;
  graphics::Shader _rt_grid_entity_binning_compute_shader;
  graphics::Shader _rng_seed_compute_shader;
  graphics::Shader _radiance_compute_shader;
  graphics::Shader _indirect_radiance_compute_shader;
  graphics::Shader _indirect_irradiance_compute_shader;
  rc::Strong<graphics::Pipeline> _grid_pipeline{};
  rc::Strong<graphics::Pipeline> _mesh_pipeline{};
  rc::Strong<graphics::Pipeline> _crosshair_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _sky_view_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _direct_radiance_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _rt_grid_entity_binning_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _rng_seed_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _radiance_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _indirect_radiance_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _indirect_irradiance_pipeline{};
  rc::Strong<graphics::Pipeline> _composite_pipeline{};
  std::optional<graphics::Image_format> _composite_pipeline_color_format{};
  rc::Strong<graphics::Image> _transmittance_lut{};
  rc::Strong<graphics::Descriptor> _transmittance_lut_sampled_descriptor{};
  rc::Strong<graphics::Image> _sky_view_lut{};
  rc::Strong<graphics::Descriptor> _sky_view_lut_sampled_descriptor{};
  rc::Strong<graphics::Descriptor> _sky_view_lut_storage_descriptor{};
  Texture_manager _texture_manager;
  Block_texture_registry _block_texture_registry;
  Block_model_registry _block_model_registry;
  std::unique_ptr<Grid_mesh> _grid_mesh;
  std::unique_ptr<Grid_mesh> _pending_grid_mesh;
  rc::Strong<graphics::Buffer> _scene_uniform_buffer{};
  std::array<rc::Strong<graphics::Buffer>, max_frames_in_flight>
    _rt_entity_buffers{};
  std::array<rc::Strong<graphics::Buffer>, max_frames_in_flight>
    _rt_entity_binning_buffers{};
  std::size_t _rt_entity_capacity{};
  rc::Strong<graphics::Buffer> _cube_vertex_buffer{};
  rc::Strong<graphics::Buffer> _cube_index_buffer{};
  rc::Strong<graphics::Buffer> _crosshair_index_buffer{};
  rc::Strong<graphics::Buffer> _composite_index_buffer{};
  u32 _frame_number{};
  float _animation_time{};
  std::optional<math::mat4> _previous_view_projection_matrix{};
};

Application::Application(Application_create_info const &create_info)
    : _impl{std::make_unique<Impl>(create_info)} {}

Application::~Application() = default;

bool Application::update(float duration) { return _impl->update(duration); }

void Application::exit() {
  _impl->exit();
  _impl.reset();
}
} // namespace fpsparty::client
