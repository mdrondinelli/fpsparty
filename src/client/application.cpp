#include "client/application.hpp"
#include "client/client.hpp"
#include "client/grid_mesh.hpp"
#include "constants.hpp"
#include "glfw.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/graphics.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/shader_stage.hpp"
#include "graphics/synchronization_scope.hpp"
#include "graphics/work_done_callback.hpp"
#include "math/transforms.hpp"
#include "math/vec.hpp"
#include "ppm/ppm.hpp"
#include <Eigen/Dense>
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
#include <tracy/Tracy.hpp>
#include <tuple>
#include <vector>
#include <vulkan/vulkan.hpp>

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

auto const crosshair_indices = std::array<std::uint16_t, 12>{0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};
auto const composite_indices = std::array<std::uint16_t, 3>{0, 1, 2};
auto const sky_color = math::vec4{0.4196f, 0.6196f, 0.7451f, 1.0f};
auto constexpr z_near = 0.1f;

struct Composite_push_constants {
  std::uint32_t albedo_texture_index;
  std::uint32_t mask_texture_index;
  std::uint32_t depth_texture_index;
  float z_near;
};

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
          graphics::load_shader("./assets/shaders/composite.frag.spv")} {
    std::cout << "Opened window.\n";
    // _graphics.set_vsync_preferred(false);
    _glfw_window->set_key_callback(this);
    _glfw_window->set_mouse_button_callback(this);
    _glfw_window->set_cursor_pos_callback(this);
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
    _placeholder_texture = upload_texture("./assets/textures/placeholder.ppm");
    std::cout << "Uploaded placeholder texture.\n";
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
    }
    update_grid_mesh();
    render();
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
    _graphics.poll_works();
    if (_pending_grid_mesh && _pending_grid_mesh->is_uploaded()) {
      _grid_mesh = std::move(_pending_grid_mesh);
    }
    auto [work_recorder, swapchain_image] = _graphics.record_frame_work();
    auto const framebuffer_extent = swapchain_image->get_extent().eval();
    auto const framebuffer_size = framebuffer_extent.head<2>().eval();
    auto [albedo_image, albedo_image_created] =
      get_albedo_image(framebuffer_extent);
    auto [crosshair_mask_image, crosshair_mask_image_created] =
      get_crosshair_mask_image(framebuffer_extent);
    auto [depth_image, depth_image_created] =
      get_depth_image(framebuffer_extent);
    auto const color_attachment_scope = graphics::Synchronization_scope{
      .stage_mask =
        graphics::Pipeline_stage_flag_bits::color_attachment_output,
      .access_mask = graphics::Access_flag_bits::color_attachment_write,
    };
    auto const sampled_image_scope = graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::fragment_shader,
      .access_mask = graphics::Access_flag_bits::shader_sampled_read,
    };
    auto record_color_attachment_transition =
      [&](rc::Strong<graphics::Image> image, bool image_created) {
        if (image_created) {
          work_recorder.transition_image_layout(
            {},
            color_attachment_scope,
            graphics::Image_layout::undefined,
            graphics::Image_layout::general,
            image);
        } else {
          work_recorder.barrier(sampled_image_scope, color_attachment_scope);
        }
      };
    record_color_attachment_transition(albedo_image, albedo_image_created);
    record_color_attachment_transition(
      crosshair_mask_image, crosshair_mask_image_created);
    auto const depth_scope = graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::early_fragment_tests |
                    graphics::Pipeline_stage_flag_bits::late_fragment_tests,
      .access_mask = graphics::Access_flag_bits::depth_stencil_attachment_read |
                     graphics::Access_flag_bits::depth_stencil_attachment_write,
    };
    if (depth_image_created) {
      work_recorder.transition_image_layout(
        {},
        depth_scope,
        graphics::Image_layout::undefined,
        graphics::Image_layout::general,
        depth_image);
    } else {
      work_recorder.barrier(sampled_image_scope, depth_scope);
    }
    work_recorder.begin_rendering({
      .color_image = albedo_image,
      .depth_image = depth_image,
      .color_clear_value = sky_color,
    });
    auto const
      [grid_pipeline, mesh_pipeline, crosshair_pipeline, composite_pipeline] =
      get_graphics_pipelines(swapchain_image->get_format());
    work_recorder.set_viewport(framebuffer_size);
    work_recorder.set_scissor(framebuffer_size);
    work_recorder.set_depth_test_enabled(true);
    work_recorder.set_depth_write_enabled(true);
    work_recorder.set_depth_compare_op(graphics::Compare_op::greater);
    auto const &session = _client.get_session();
    auto const camera =
      session && _local_player && _local_player->player_entity_id &&
          _local_player->humanoid_entity_id
        ? session->get_scene()
            .get_interpolated_camera(*_local_player->player_entity_id)
        : nullptr;
    if (camera) {
      auto const view_matrix =
        (math::x_rotation_matrix(-_local_player->input_state.pitch) *
         math::y_rotation_matrix(-_local_player->input_state.yaw) *
         math::translation_matrix(-camera->position))
          .eval();
      auto const zoom = 1.5f;
      auto const aspect_ratio =
        static_cast<float>(swapchain_image->get_extent().x()) /
        static_cast<float>(swapchain_image->get_extent().y());
      auto const projection_matrix = math::perspective_projection_matrix(
        aspect_ratio > 1.0f ? zoom : zoom * aspect_ratio,
        aspect_ratio > 1.0f ? zoom / aspect_ratio : zoom,
        z_near);
      auto const view_projection_matrix =
        (projection_matrix * view_matrix).eval();
      // draw grid
      if (_grid_mesh && _grid_mesh->is_uploaded()) {
        auto const texture_index =
          work_recorder.upload_sampled_image_descriptor(_placeholder_texture);
        work_recorder.bind_pipeline(grid_pipeline);
        work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
        work_recorder.set_cull_mode(graphics::Cull_mode::back);
        work_recorder.bind_index_buffer(
          _grid_mesh->get_index_buffer(), graphics::Index_type::u32);
        work_recorder
          .push_data(0, std::as_bytes(std::span{&view_projection_matrix, 1}));
        work_recorder
          .push_buffer_device_address(80, _grid_mesh->get_vertex_buffer());
        work_recorder
          .push_data(88, std::as_bytes(std::span{&texture_index, 1}));
        auto record_normal_push_constant = [&](math::vec4 const &value) {
          work_recorder.push_data(64, std::as_bytes(std::span{&value, 1}));
        };
        record_normal_push_constant({1.0f, 0.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, game::Axis::x, Sign::positive);
        record_normal_push_constant({-1.0f, 0.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, game::Axis::x, Sign::negative);
        record_normal_push_constant({0.0f, 1.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, game::Axis::y, Sign::positive);
        record_normal_push_constant({0.0f, -1.0f, 0.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, game::Axis::y, Sign::negative);
        record_normal_push_constant({0.0f, 0.0f, 1.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, game::Axis::z, Sign::positive);
        record_normal_push_constant({0.0f, 0.0f, -1.0f, 0.0f});
        _grid_mesh->record_draws(work_recorder, game::Axis::z, Sign::negative);
      }
      // draw cubes
      work_recorder.bind_pipeline(mesh_pipeline);
      work_recorder.set_cull_mode(graphics::Cull_mode::back);
      work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
      work_recorder
        .bind_index_buffer(_cube_index_buffer, graphics::Index_type::u16);
      work_recorder.push_buffer_device_address(64, _cube_vertex_buffer);
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
    }
    work_recorder.end_rendering();

    work_recorder.begin_rendering({
      .color_image = crosshair_mask_image,
    });
    work_recorder.set_viewport(framebuffer_size);
    work_recorder.set_scissor(framebuffer_size);
    work_recorder.bind_pipeline(crosshair_pipeline);
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

    work_recorder.barrier(color_attachment_scope, sampled_image_scope);
    work_recorder.barrier(depth_scope, sampled_image_scope);
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
    auto const albedo_texture_index =
      work_recorder.upload_sampled_image_descriptor(albedo_image);
    auto const mask_texture_index =
      work_recorder.upload_sampled_image_descriptor(crosshair_mask_image);
    auto const depth_texture_index =
      work_recorder.upload_sampled_image_descriptor(depth_image);
    auto const composite_push_constants = Composite_push_constants{
      .albedo_texture_index = albedo_texture_index,
      .mask_texture_index = mask_texture_index,
      .depth_texture_index = depth_texture_index,
      .z_near = z_near,
    };
    work_recorder.bind_pipeline(composite_pipeline);
    work_recorder.set_cull_mode(graphics::Cull_mode::none);
    work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
    work_recorder
      .bind_index_buffer(_composite_index_buffer, graphics::Index_type::u16);
    work_recorder.push_data(
      0, std::as_bytes(std::span{&composite_push_constants, 1}));
    work_recorder.draw_indexed({
      .index_count = static_cast<std::uint32_t>(composite_indices.size()),
      .instance_count = 1,
      .first_index = 0,
      .vertex_offset = 0,
      .first_instance = 0,
    });
    work_recorder.end_rendering();
    _graphics.submit_frame_work(std::move(work_recorder));
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
    });
    scene.reset_grid_remesh_flag();
  }

  std::pair<rc::Strong<graphics::Image>, bool>
  get_albedo_image(math::ivec3 const &extent) {
    return get_color_image(
      _albedo_image,
      graphics::Image_format::b8g8r8a8_srgb,
      extent);
  }

  std::pair<rc::Strong<graphics::Image>, bool>
  get_crosshair_mask_image(math::ivec3 const &extent) {
    return get_color_image(
      _crosshair_mask_image, graphics::Image_format::r8_unorm, extent);
  }

  std::pair<rc::Strong<graphics::Image>, bool> get_color_image(
    rc::Strong<graphics::Image> &image,
    graphics::Image_format format,
    math::ivec3 const &extent) {
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
    }
    return {image, create_image};
  }

  std::pair<rc::Strong<graphics::Image>, bool>
  get_depth_image(math::ivec3 const &extent) {
    auto const create_image =
      !_depth_image || _depth_image->get_extent() != extent;
    if (create_image) {
      _depth_image = _graphics.create_image({
        .dimensionality = 2,
        .format = graphics::Image_format::d32_sfloat,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::sampled |
                 graphics::Image_usage_flag_bits::depth_attachment,
      });
    }
    return {_depth_image, create_image};
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

  std::tuple<
    rc::Strong<graphics::Pipeline>,
    rc::Strong<graphics::Pipeline>,
    rc::Strong<graphics::Pipeline>,
    rc::Strong<graphics::Pipeline>>
  get_graphics_pipelines(graphics::Image_format swapchain_image_format) {
    ZoneScoped;
    if (!_grid_pipeline) {
      _grid_pipeline = make_grid_pipeline();
    }
    if (!_mesh_pipeline) {
      _mesh_pipeline = make_mesh_pipeline();
    }
    if (!_crosshair_pipeline) {
      _crosshair_pipeline = make_crosshair_pipeline();
    }
    if (
      !_composite_pipeline_color_format ||
      *_composite_pipeline_color_format != swapchain_image_format) {
      _composite_pipeline = make_composite_pipeline(swapchain_image_format);
      _composite_pipeline_color_format = swapchain_image_format;
    }
    return {
      _grid_pipeline, _mesh_pipeline, _crosshair_pipeline, _composite_pipeline};
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
      graphics::Image_format::b8g8r8a8_srgb;
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
      graphics::Image_format::b8g8r8a8_srgb;
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

  State _state{State::initial};
  Client _client;
  Local_player *_local_player{};
  enet::Address _server_address;
  glfw::Unique_window _glfw_window{};
  vk::UniqueSurfaceKHR _vk_surface{};
  graphics::Graphics _graphics{};
  rc::Strong<graphics::Image> _depth_image{};
  rc::Strong<graphics::Image> _albedo_image{};
  rc::Strong<graphics::Image> _crosshair_mask_image{};
  rc::Strong<graphics::Image> _placeholder_texture{};
  graphics::Shader _grid_vertex_shader;
  graphics::Shader _grid_fragment_shader;
  graphics::Shader _mesh_vertex_shader;
  graphics::Shader _mesh_fragment_shader;
  graphics::Shader _crosshair_vertex_shader;
  graphics::Shader _crosshair_fragment_shader;
  graphics::Shader _composite_vertex_shader;
  graphics::Shader _composite_fragment_shader;
  rc::Strong<graphics::Pipeline> _grid_pipeline{};
  rc::Strong<graphics::Pipeline> _mesh_pipeline{};
  rc::Strong<graphics::Pipeline> _crosshair_pipeline{};
  rc::Strong<graphics::Pipeline> _composite_pipeline{};
  std::optional<graphics::Image_format> _composite_pipeline_color_format{};
  std::unique_ptr<Grid_mesh> _grid_mesh;
  std::unique_ptr<Grid_mesh> _pending_grid_mesh;
  rc::Strong<graphics::Buffer> _cube_vertex_buffer{};
  rc::Strong<graphics::Buffer> _cube_index_buffer{};
  rc::Strong<graphics::Buffer> _crosshair_index_buffer{};
  rc::Strong<graphics::Buffer> _composite_index_buffer{};
};

Application::Application(Application_create_info const &create_info)
    : _impl{std::make_unique<Impl>(create_info)} {}

Application::~Application() = default;

bool Application::update(float duration) { return _impl->update(duration); }
} // namespace fpsparty::client
