#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
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
  float x;
  float y;
  float z;
  float r;
  float g;
  float b;
};

auto const floor_mesh_vertices = std::vector<Vertex>{
  {.x = 10.0f, .y = 0.0f, .z = 10.0f, .r = 1.0f, .g = 0.0f, .b = 0.0f},
  {.x = 10.0f, .y = 0.0f, .z = -10.0f, .r = 0.0f, .g = 0.0f, .b = 1.0f},
  {.x = -10.0f, .y = 0.0f, .z = 10.0f, .r = 0.0f, .g = 1.0f, .b = 0.0f},
  {.x = -10.0f, .y = 0.0f, .z = -10.0f, .r = 1.0f, .g = 1.0f, .b = 0.0f},
};

auto const floor_mesh_indices = std::vector<std::uint16_t>{0, 1, 2, 3, 2, 1};

auto const cube_mesh_vertices = std::vector<Vertex>{
  // +x face
  {.x = 0.5f, .y = 0.5f, .z = 0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f},   // 1
  {.x = 0.5f, .y = -0.5f, .z = 0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f},  // 2
  {.x = 0.5f, .y = 0.5f, .z = -0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f},  // 3
  {.x = 0.5f, .y = -0.5f, .z = -0.5f, .r = 1.0f, .g = 1.0f, .b = 0.0f}, // 4
  // -x face
  {.x = -0.5f, .y = 0.5f, .z = -0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f},  // 1
  {.x = -0.5f, .y = -0.5f, .z = -0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f}, // 2
  {.x = -0.5f, .y = 0.5f, .z = 0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f},   // 3
  {.x = -0.5f, .y = -0.5f, .z = 0.5f, .r = 1.0f, .g = 1.0f, .b = 0.0f},  // 4
  // +y face
  {.x = 0.5f, .y = 0.5f, .z = 0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f},   // 1
  {.x = 0.5f, .y = 0.5f, .z = -0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f},  // 2
  {.x = -0.5f, .y = 0.5f, .z = 0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f},  // 3
  {.x = -0.5f, .y = 0.5f, .z = -0.5f, .r = 1.0f, .g = 1.0f, .b = 0.0f}, // 4
  // -y face
  {.x = 0.5f, .y = -0.5f, .z = -0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f},  // 2
  {.x = 0.5f, .y = -0.5f, .z = 0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f},   // 1
  {.x = -0.5f, .y = -0.5f, .z = -0.5f, .r = 1.0f, .g = 1.0f, .b = 0.0f}, // 4
  {.x = -0.5f, .y = -0.5f, .z = 0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f},  // 3
  // +z face
  {.x = -0.5f, .y = 0.5f, .z = 0.5f, .r = 1.0f, .g = 1.0f, .b = 0.0f},  // 3
  {.x = -0.5f, .y = -0.5f, .z = 0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f}, // 4
  {.x = 0.5f, .y = 0.5f, .z = 0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f},   // 1
  {.x = 0.5f, .y = -0.5f, .z = 0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f},  // 2
  // -z face
  {.x = 0.5f, .y = 0.5f, .z = -0.5f, .r = 0.0f, .g = 0.0f, .b = 1.0f},   // 1
  {.x = 0.5f, .y = -0.5f, .z = -0.5f, .r = 1.0f, .g = 0.0f, .b = 0.0f},  // 2
  {.x = -0.5f, .y = 0.5f, .z = -0.5f, .r = 1.0f, .g = 1.0f, .b = 0.0f},  // 3
  {.x = -0.5f, .y = -0.5f, .z = -0.5f, .r = 0.0f, .g = 1.0f, .b = 0.0f}, // 4
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
auto constexpr transmittance_lut_width = 256;
auto constexpr transmittance_lut_height = 1024;

struct Transmittance_push_constants {
  math::ivec2 lut_size;
  i32 lut_index;
};

static_assert(sizeof(Transmittance_push_constants) == 12);
static_assert(offsetof(Transmittance_push_constants, lut_size) == 0);
static_assert(offsetof(Transmittance_push_constants, lut_index) == 8);

struct Sky_push_constants {
  math::mat4 camera_basis;
  math::vec2 zoom;
  f32 altitude;
  u32 transmittance_lut;
  alignas(16) math::vec3 sun_direction;
  alignas(16) math::vec3 sun_irradiance;
};

static_assert(sizeof(Sky_push_constants) == 112);
static_assert(offsetof(Sky_push_constants, camera_basis) == 0);
static_assert(offsetof(Sky_push_constants, zoom) == 64);
static_assert(offsetof(Sky_push_constants, altitude) == 72);
static_assert(offsetof(Sky_push_constants, transmittance_lut) == 76);
static_assert(offsetof(Sky_push_constants, sun_direction) == 80);
static_assert(offsetof(Sky_push_constants, sun_irradiance) == 96);

struct Composite_push_constants {
  u32 radiance_texture_index;
  u32 mask_texture_index;
  u32 depth_texture_index;
  f32 z_near;
  u32 frame_number;
};

static_assert(sizeof(Composite_push_constants) == 20);
static_assert(offsetof(Composite_push_constants, radiance_texture_index) == 0);
static_assert(offsetof(Composite_push_constants, mask_texture_index) == 4);
static_assert(offsetof(Composite_push_constants, depth_texture_index) == 8);
static_assert(offsetof(Composite_push_constants, z_near) == 12);
static_assert(offsetof(Composite_push_constants, frame_number) == 16);

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
        _sky_vertex_shader{
          graphics::load_shader("./assets/shaders/atmosphere/sky.vert.spv")},
        _sky_fragment_shader{
          graphics::load_shader("./assets/shaders/atmosphere/sky.frag.spv")},
        _grid_pipeline{make_grid_pipeline()},
        _mesh_pipeline{make_mesh_pipeline()},
        _crosshair_pipeline{make_crosshair_pipeline()},
        _sky_pipeline{make_sky_pipeline()},
        _texture_manager{{.graphics = &_graphics}},
        _block_texture_registry{{.graphics = &_graphics}} {
    std::cout << "Opened window.\n";
    // _graphics.set_vsync_preferred(false);
    _glfw_window->set_key_callback(this);
    _glfw_window->set_mouse_button_callback(this);
    _glfw_window->set_cursor_pos_callback(this);
    init_transmittance_lut();
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
    if (_state == State::connected) {
      if (!_client.is_connected()) {
        _state = State::stopped;
        return false;
      }
      _client.update(duration);
      _animation_time += duration;
    }
    _graphics.poll_works();
    update_grid_mesh();
    render();
    ++_frame_number;
    return true;
  }

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
    }
    auto [work_recorder, swapchain_image] =
      _graphics.record_frame_work({.descriptor_capacity = 32});
    auto const framebuffer_extent = swapchain_image->get_extent().eval();
    auto const framebuffer_size = framebuffer_extent.head<2>().eval();
    get_color_render_target(
      work_recorder,
      _radiance_render_target,
      graphics::Image_format::r16g16b16a16_sfloat,
      framebuffer_extent);
    get_color_render_target(
      work_recorder,
      _crosshair_mask_render_target,
      graphics::Image_format::r8_unorm,
      framebuffer_extent);
    get_depth_render_target(
      work_recorder, _depth_render_target, framebuffer_extent);
    record_forward_pass(work_recorder, framebuffer_size);
    record_crosshair_pass(work_recorder, framebuffer_size);
    record_composite_pass(work_recorder, swapchain_image, framebuffer_size);
    _graphics.submit_frame_work(std::move(work_recorder));
  }

  void record_forward_pass(
    graphics::Work_recorder &work_recorder, math::ivec2 framebuffer_size) {
    ZoneScoped;
    work_recorder.barrier(
      sampled_image_scope, color_attachment_scope | depth_attachment_scope);
    work_recorder.begin_rendering({
      .color_image = _radiance_render_target,
      .depth_image = _depth_render_target,
      .color_clear_value = sky_color,
    });
    work_recorder.set_viewport(framebuffer_size);
    work_recorder.set_scissor(framebuffer_size);
    auto const &session = _client.get_session();
    auto const camera =
      session && _local_player && _local_player->player_entity_id &&
          _local_player->humanoid_entity_id
        ? session->get_scene()
            .get_interpolated_camera(*_local_player->player_entity_id)
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
      auto const zoom = 1.25f;
      auto const aspect_ratio = static_cast<float>(framebuffer_size.x()) /
                                static_cast<float>(framebuffer_size.y());
      auto const projection_matrix = math::perspective_projection_matrix(
        aspect_ratio > 1.0f ? zoom : zoom * aspect_ratio,
        aspect_ratio > 1.0f ? zoom / aspect_ratio : zoom,
        z_near);
      auto const view_projection_matrix =
        (projection_matrix * view_matrix).eval();
      auto const sun_direction =
        session->get_scene().get_interpolated_sun_direction();
      // draw grid
      if (_grid_mesh && _grid_mesh->is_uploaded()) {
        work_recorder.bind_pipeline(_grid_pipeline);
        work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
        work_recorder.set_cull_mode(graphics::Cull_mode::back);
        work_recorder.bind_index_buffer(
          _grid_mesh->get_index_buffer(), graphics::Index_type::u32);
        work_recorder.push_buffer(0, _grid_mesh->get_vertex_buffer());
        work_recorder.push_buffer(
          8, _block_texture_registry.get_descriptor_index_buffer());
        _block_texture_registry.upload_descriptors(work_recorder);
        work_recorder
          .push_data(16, std::as_bytes(std::span{&view_projection_matrix, 1}));
        work_recorder
          .push_data(80, std::as_bytes(std::span{&sun_direction, 1}));
        work_recorder.push_data(92, std::as_bytes(std::span{&_animation_time, 1}));
        auto push_normal = [&](math::vec4 const &value) {
          work_recorder.push_data(96, std::as_bytes(std::span{&value, 1}));
        };
        push_normal({1.0f, 0.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, +math::axis3::x);
        push_normal({-1.0f, 0.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, -math::axis3::x);
        push_normal({0.0f, 1.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, +math::axis3::y);
        push_normal({0.0f, -1.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, -math::axis3::y);
        push_normal({0.0f, 0.0f, 1.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, +math::axis3::z);
        push_normal({0.0f, 0.0f, -1.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, -math::axis3::z);
      }
      // draw cubes
      work_recorder.bind_pipeline(_mesh_pipeline);
      work_recorder.set_cull_mode(graphics::Cull_mode::back);
      work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
      work_recorder
        .bind_index_buffer(_cube_index_buffer, graphics::Index_type::u16);
      work_recorder.push_buffer(64, _cube_vertex_buffer);
      for (auto const &[id, instance] :
           session->get_scene().get_interpolated_mesh_instances()) {
        auto rotation_matrix = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
        rotation_matrix.block<3, 3>(0, 0) =
          instance.orientation.toRotationMatrix();
        auto const model_matrix =
          (math::translation_matrix(instance.position) * rotation_matrix *
           math::axis_aligned_scale_matrix(instance.scale))
            .eval();
        auto const model_view_projection_matrix =
          (view_projection_matrix * model_matrix).eval();
        work_recorder.push_data(
          0, std::as_bytes(std::span{&model_view_projection_matrix, 1}));
        work_recorder.draw_indexed({
          .index_count = static_cast<std::uint32_t>(cube_mesh_indices.size()),
          .instance_count = 1,
          .first_index = 0,
          .vertex_offset = 0,
          .first_instance = 0,
        });
      }
      record_sky_draw(
        work_recorder,
        framebuffer_size,
        camera->position,
        session->get_scene().get_interpolated_sun_direction(),
        math::vec3::Constant(1300.0f));
    }
    work_recorder.end_rendering();
  }

  void record_sky_draw(
    graphics::Work_recorder &work_recorder,
    math::ivec2 framebuffer_size,
    math::vec3 camera_position,
    math::vec3 sun_direction,
    math::vec3 sun_irradiance) {
    ZoneScoped;
    auto constexpr zoom = 1.25f;
    auto const aspect_ratio = static_cast<f32>(framebuffer_size.x()) /
                              static_cast<f32>(framebuffer_size.y());
    auto const zoom_vec = math::vec2{
      aspect_ratio > 1.0f ? zoom : zoom * aspect_ratio,
      aspect_ratio > 1.0f ? zoom / aspect_ratio : zoom,
    };
    auto const transmittance_lut =
      work_recorder.upload_sampled_image_descriptor(_transmittance_lut);
    auto const push_constants = Sky_push_constants{
      .camera_basis =
        math::y_rotation_matrix(_local_player->input_state.yaw) *
        math::x_rotation_matrix(_local_player->input_state.pitch),
      .zoom = zoom_vec,
      .altitude = camera_position.y() + 84.0f,
      .transmittance_lut = transmittance_lut,
      .sun_direction = sun_direction,
      .sun_irradiance = sun_irradiance,
    };
    work_recorder.bind_pipeline(_sky_pipeline);
    work_recorder.set_viewport(framebuffer_size);
    work_recorder.set_scissor(framebuffer_size);
    work_recorder.set_cull_mode(graphics::Cull_mode::none);
    work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
    work_recorder.set_depth_test_enabled(true);
    work_recorder.set_depth_write_enabled(false);
    work_recorder.set_depth_compare_op(graphics::Compare_op::equal);
    work_recorder
      .bind_index_buffer(_composite_index_buffer, graphics::Index_type::u16);
    work_recorder
      .push_data(0, std::as_bytes(std::span{&push_constants, 1}));
    work_recorder.draw_indexed({
      .index_count = static_cast<u32>(composite_indices.size()),
      .instance_count = 1,
      .first_index = 0,
      .vertex_offset = 0,
      .first_instance = 0,
    });
  }

  void record_crosshair_pass(
    graphics::Work_recorder &work_recorder, math::ivec2 framebuffer_size) {
    ZoneScoped;
    work_recorder.begin_rendering({
      .color_image = _crosshair_mask_render_target,
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
    ZoneScoped;
    work_recorder.barrier(
      color_attachment_scope | depth_attachment_scope, sampled_image_scope);
    work_recorder.transition_image_layout(
      {},
      color_attachment_scope,
      graphics::Image_layout::undefined,
      graphics::Image_layout::general,
      swapchain_image);
    work_recorder.begin_rendering({
      .color_image = swapchain_image,
    });
    work_recorder.set_viewport(framebuffer_size);
    work_recorder.set_scissor(framebuffer_size);
    auto const radiance_texture_index =
      work_recorder.upload_sampled_image_descriptor(_radiance_render_target);
    auto const crosshair_mask_texture_index =
      work_recorder
        .upload_sampled_image_descriptor(_crosshair_mask_render_target);
    auto const depth_texture_index =
      work_recorder.upload_sampled_image_descriptor(_depth_render_target);
    auto const composite_push_constants = Composite_push_constants{
      .radiance_texture_index = radiance_texture_index,
      .mask_texture_index = crosshair_mask_texture_index,
      .depth_texture_index = depth_texture_index,
      .z_near = z_near,
      .frame_number = _frame_number,
    };
    work_recorder
      .bind_pipeline(get_composite_pipeline(swapchain_image->get_format()));
    work_recorder.set_cull_mode(graphics::Cull_mode::none);
    work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
    work_recorder
      .bind_index_buffer(_composite_index_buffer, graphics::Index_type::u16);
    work_recorder
      .push_data(0, std::as_bytes(std::span{&composite_push_constants, 1}));
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
    ZoneScoped;
    auto transmittance_shader = graphics::load_shader(
      "./assets/shaders/atmosphere/transmittance.comp.spv");
    auto transmittance_pipeline =
      _graphics.create_compute_pipeline({.shader = &transmittance_shader});
    auto const lut_size = math::ivec2{
      transmittance_lut_width,
      transmittance_lut_height,
    };
    _transmittance_lut = _graphics.create_image({
      .dimensionality = 2,
      .format = graphics::Image_format::r16g16b16a16_sfloat,
      .extent = {lut_size.x(), lut_size.y(), 1},
      .mip_level_count = 1,
      .array_layer_count = 1,
      .usage = graphics::Image_usage_flag_bits::sampled |
               graphics::Image_usage_flag_bits::storage,
    });
    auto work_recorder =
      _graphics.record_transient_work({.descriptor_capacity = 1});
    work_recorder.transition_image_layout(
      {},
      compute_shader_storage_write_scope,
      graphics::Image_layout::undefined,
      graphics::Image_layout::general,
      _transmittance_lut);
    auto const lut_index =
      work_recorder.upload_storage_image_descriptor(_transmittance_lut);
    auto const push_constants = Transmittance_push_constants{
      .lut_size = lut_size,
      .lut_index = static_cast<i32>(lut_index),
    };
    work_recorder.bind_compute_pipeline(transmittance_pipeline);
    work_recorder
      .push_data(0, std::as_bytes(std::span{&push_constants, 1}));
    work_recorder.dispatch(lut_size.x(), lut_size.y(), 1);
    work_recorder.barrier(
      compute_shader_storage_write_scope, sampled_image_scope);
    auto work = _graphics.submit_transient_work(std::move(work_recorder));
    work->await();
  }

  void get_color_render_target(
    graphics::Work_recorder &work_recorder,
    rc::Strong<graphics::Image> &image,
    graphics::Image_format format,
    math::ivec3 extent) {
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
    }
  }

  void get_depth_render_target(
    graphics::Work_recorder &work_recorder,
    rc::Strong<graphics::Image> &image,
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
    }
  }

  rc::Strong<graphics::Buffer>
  upload_vertices(std::span<std::byte const> data) {
    auto const staging_buffer = _graphics.create_staging_buffer(data);
    auto vertex_buffer = _graphics.create_buffer({
      .size = data.size(),
      .usage = graphics::Buffer_usage_flag_bits::transfer_dst |
               graphics::Buffer_usage_flag_bits::shader_device_address,
    });
    auto work_recorder = _graphics.record_transient_work({});
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
    auto work_recorder = _graphics.record_transient_work({});
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
    auto work_recorder = _graphics.record_transient_work({});
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
    auto const color_attachment_format =
      graphics::Image_format::r16g16b16a16_sfloat;
    return _graphics.create_pipeline({
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
          .color_attachment_formats = {&color_attachment_format, 1},
        },
    });
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
    auto const color_attachment_format =
      graphics::Image_format::r16g16b16a16_sfloat;
    return _graphics.create_pipeline({
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
          .color_attachment_formats = {&color_attachment_format, 1},
        },
    });
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
    return _graphics.create_pipeline({
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
  }

  rc::Strong<graphics::Pipeline> make_sky_pipeline() {
    auto const shader_stages =
      std::vector<graphics::Pipeline_shader_stage_create_info>{
        {
          .stage = graphics::Shader_stage_flag_bits::vertex,
          .shader = &_sky_vertex_shader,
        },
        {
          .stage = graphics::Shader_stage_flag_bits::fragment,
          .shader = &_sky_fragment_shader,
        },
      };
    auto const color_attachment_format =
      graphics::Image_format::r16g16b16a16_sfloat;
    return _graphics.create_pipeline({
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
          .color_attachment_formats = {&color_attachment_format, 1},
        },
    });
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
    return _graphics.create_pipeline({
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
  }

  static auto constexpr sampled_image_scope = graphics::Synchronization_scope{
    .stage_mask = graphics::Pipeline_stage_flag_bits::fragment_shader,
    .access_mask = graphics::Access_flag_bits::shader_sampled_read,
  };

  static auto constexpr compute_shader_storage_write_scope =
    graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
      .access_mask = graphics::Access_flag_bits::shader_storage_write,
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
  rc::Strong<graphics::Image> _depth_render_target{};
  rc::Strong<graphics::Image> _radiance_render_target{};
  rc::Strong<graphics::Image> _crosshair_mask_render_target{};
  graphics::Shader _grid_vertex_shader;
  graphics::Shader _grid_fragment_shader;
  graphics::Shader _mesh_vertex_shader;
  graphics::Shader _mesh_fragment_shader;
  graphics::Shader _crosshair_vertex_shader;
  graphics::Shader _crosshair_fragment_shader;
  graphics::Shader _composite_vertex_shader;
  graphics::Shader _composite_fragment_shader;
  graphics::Shader _sky_vertex_shader;
  graphics::Shader _sky_fragment_shader;
  rc::Strong<graphics::Pipeline> _grid_pipeline{};
  rc::Strong<graphics::Pipeline> _mesh_pipeline{};
  rc::Strong<graphics::Pipeline> _crosshair_pipeline{};
  rc::Strong<graphics::Pipeline> _sky_pipeline{};
  rc::Strong<graphics::Pipeline> _composite_pipeline{};
  std::optional<graphics::Image_format> _composite_pipeline_color_format{};
  rc::Strong<graphics::Image> _transmittance_lut{};
  Texture_manager _texture_manager;
  Block_texture_registry _block_texture_registry;
  Block_model_registry _block_model_registry;
  std::unique_ptr<Grid_mesh> _grid_mesh;
  std::unique_ptr<Grid_mesh> _pending_grid_mesh;
  rc::Strong<graphics::Buffer> _cube_vertex_buffer{};
  rc::Strong<graphics::Buffer> _cube_index_buffer{};
  rc::Strong<graphics::Buffer> _crosshair_index_buffer{};
  rc::Strong<graphics::Buffer> _composite_index_buffer{};
  u32 _frame_number{};
  float _animation_time{};
};

Application::Application(Application_create_info const &create_info)
    : _impl{std::make_unique<Impl>(create_info)} {}

Application::~Application() = default;

bool Application::update(float duration) { return _impl->update(duration); }
} // namespace fpsparty::client
