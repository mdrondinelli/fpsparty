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
#include "math/transformation_matrices.hpp"
#include <Eigen/Dense>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
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

vk::UniqueSurfaceKHR make_vk_surface(glfw::Window window) {
  auto retval = glfw::create_window_surface_unique(
    graphics::Global_vulkan_state::get().instance(), window);
  std::cout << "Created VkSurfaceKHR.\n";
  return retval;
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
          .vsync_preferred = false,
        }},
        _grid_vertex_shader{
          graphics::load_shader("./assets/shaders/grid.vert.spv")},
        _grid_fragment_shader{
          graphics::load_shader("./assets/shaders/grid.frag.spv")},
        _mesh_vertex_shader{
          graphics::load_shader("./assets/shaders/shader.vert.spv")},
        _mesh_fragment_shader{
          graphics::load_shader("./assets/shaders/shader.frag.spv")} {
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
  }

  ~Impl() {
    _glfw_window->set_key_callback(nullptr);
    _glfw_window->set_mouse_button_callback(nullptr);
    _glfw_window->set_cursor_pos_callback(nullptr);
  }

  bool update(float duration) {
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
      _client.tick(duration);
    }
    update_grid_mesh();
    render();
    return true;
  }

private:
  enum class State {
    initial,
    connecting,
    connected,
    stopped,
  };

  void render() {
    ZoneScoped;
    _graphics.poll_works();
    if (_pending_grid_mesh && _pending_grid_mesh->is_uploaded()) {
      _grid_mesh = std::move(_pending_grid_mesh);
    }
    auto [work_recorder, swapchain_image] = _graphics.record_frame_work();
    work_recorder.transition_image_layout(
      {},
      {
        .stage_mask =
          graphics::Pipeline_stage_flag_bits::color_attachment_output,
        .access_mask = graphics::Access_flag_bits::color_attachment_write,
      },
      graphics::Image_layout::undefined,
      graphics::Image_layout::general,
      swapchain_image);
    auto [depth_image, depth_image_created] =
      get_depth_image(swapchain_image->get_extent());
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
      work_recorder.barrier(
        {
          .stage_mask =
            graphics::Pipeline_stage_flag_bits::early_fragment_tests |
            graphics::Pipeline_stage_flag_bits::late_fragment_tests,
          .access_mask =
            graphics::Access_flag_bits::depth_stencil_attachment_write,
        },
        depth_scope);
    }
    work_recorder.begin_rendering({
      .color_image = swapchain_image,
      .depth_image = depth_image,
    });
    auto const [grid_pipeline, mesh_pipeline] =
      get_graphics_pipelines(swapchain_image->get_format());
    work_recorder.set_viewport(swapchain_image->get_extent().head<2>());
    work_recorder.set_scissor(swapchain_image->get_extent().head<2>());
    work_recorder.set_depth_test_enabled(true);
    work_recorder.set_depth_write_enabled(true);
    work_recorder.set_depth_compare_op(graphics::Compare_op::greater);
    auto const scene = _client.get_scene();
    auto const camera = _client.get_camera_snapshot();
    if (scene && camera) {
      auto const view_matrix =
        (math::x_rotation_matrix(-camera->pitch) *
         math::y_rotation_matrix(-camera->yaw) *
         math::translation_matrix(-camera->position))
          .eval();
      auto const aspect_ratio =
        static_cast<float>(swapchain_image->get_extent().x()) /
        static_cast<float>(swapchain_image->get_extent().y());
      auto const projection_matrix = math::perspective_projection_matrix(
        aspect_ratio > 1.0f ? 1.0f : aspect_ratio,
        aspect_ratio > 1.0f ? 1.0f / aspect_ratio : 1.0f,
        0.1f);
      auto const view_projection_matrix =
        (projection_matrix * view_matrix).eval();
      // draw grid
      if (_grid_mesh && _grid_mesh->is_uploaded()) {
        work_recorder.bind_pipeline(grid_pipeline);
        work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
        work_recorder.set_cull_mode(graphics::Cull_mode::back);
        work_recorder.bind_index_buffer(
          _grid_mesh->get_index_buffer(), graphics::Index_type::u32);
        work_recorder.push_constants(
          grid_pipeline->get_layout(),
          graphics::Shader_stage_flag_bits::vertex,
          0,
          std::as_bytes(std::span{&view_projection_matrix, 1}));
        work_recorder.push_buffer_device_address(
          grid_pipeline->get_layout(),
          graphics::Shader_stage_flag_bits::vertex,
          80,
          _grid_mesh->get_vertex_buffer());
        auto record_normal_push_constant = [&](Eigen::Vector4f const &value) {
          work_recorder.push_constants(
            grid_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex |
              graphics::Shader_stage_flag_bits::fragment,
            64,
            std::as_bytes(std::span{&value, 1}));
        };
        record_normal_push_constant({1.0f, 0.0f, 0.0f, 0.0f});
        _grid_mesh
          ->record_draws(work_recorder, game::Axis::x, Sign::positive);
        record_normal_push_constant({-1.0f, 0.0f, 0.0f, 0.0f});
        _grid_mesh
          ->record_draws(work_recorder, game::Axis::x, Sign::negative);
        record_normal_push_constant({0.0f, 1.0f, 0.0f, 0.0f});
        _grid_mesh
          ->record_draws(work_recorder, game::Axis::y, Sign::positive);
        record_normal_push_constant({0.0f, -1.0f, 0.0f, 0.0f});
        _grid_mesh
          ->record_draws(work_recorder, game::Axis::y, Sign::negative);
        record_normal_push_constant({0.0f, 0.0f, 1.0f, 0.0f});
        _grid_mesh
          ->record_draws(work_recorder, game::Axis::z, Sign::positive);
        record_normal_push_constant({0.0f, 0.0f, -1.0f, 0.0f});
        _grid_mesh
          ->record_draws(work_recorder, game::Axis::z, Sign::negative);
      }
      // draw cubes
      work_recorder.bind_pipeline(mesh_pipeline);
      work_recorder.set_cull_mode(graphics::Cull_mode::back);
      work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
      work_recorder
        .bind_index_buffer(_cube_index_buffer, graphics::Index_type::u16);
      work_recorder.push_buffer_device_address(
        mesh_pipeline->get_layout(),
        graphics::Shader_stage_flag_bits::vertex,
        64,
        _cube_vertex_buffer);
      for (auto const &instance : scene->get_mesh_instances()) {
        auto rotation_matrix = Eigen::Matrix4f{Eigen::Matrix4f::Identity()};
        rotation_matrix.block<3, 3>(0, 0) =
          instance.orientation.toRotationMatrix();
        auto const model_matrix =
          (math::translation_matrix(instance.position) *
           rotation_matrix *
           math::axis_aligned_scale_matrix(instance.scale))
            .eval();
        auto const model_view_projection_matrix =
          (view_projection_matrix * model_matrix).eval();
        work_recorder.push_constants(
          _mesh_pipeline->get_layout(),
          graphics::Shader_stage_flag_bits::vertex,
          0,
          std::as_bytes(std::span{&model_view_projection_matrix, 1}));
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
    _graphics.submit_frame_work(std::move(work_recorder));
  }

  void on_key(
    glfw::Window, glfw::Key key, int, glfw::Press_action action, int) override {
    if (key == glfw::Key::k_escape && action == glfw::Press_action::press) {
      _glfw_window->set_cursor_input_mode(glfw::Cursor_input_mode::normal);
    } else if (_client.has_scene()) {
      auto input_state = _client.get_current_input_state();
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
        input_state.crouch = action != glfw::Press_action::release;
        break;
      default:
      }
      _client.set_current_input_state(input_state);
    }
  }

  void on_mouse_button(
    glfw::Window,
    glfw::Mouse_button button,
    glfw::Press_state action,
    int) override {
    if (_client.has_scene()) {
      auto input_state = _client.get_current_input_state();
      if (button == glfw::Mouse_button::mb_left) {
        input_state.use_primary = action != glfw::Press_state::release;
      } else if (button == glfw::Mouse_button::mb_right) {
        input_state.use_secondary = action != glfw::Press_state::release;
      }
      _client.set_current_input_state(input_state);
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
      _client.has_scene() && _glfw_window->get_cursor_input_mode() ==
                       glfw::Cursor_input_mode::disabled) {
      auto input_state = _client.get_current_input_state();
      input_state.yaw -=
        static_cast<float>(dxpos * constants::mouselook_sensititvity);
      input_state.pitch +=
        static_cast<float>(dypos * constants::mouselook_sensititvity);
      input_state.pitch = std::clamp(
        input_state.pitch,
        -0.5f * std::numbers::pi_v<float>,
        0.5f * std::numbers::pi_v<float>);
      _client.set_current_input_state(input_state);
    }
  }

  void update_grid_mesh() {
    auto scene = _client.get_scene();
    if (!scene) {
      _grid_mesh.reset();
      _pending_grid_mesh.reset();
      return;
    }
    if (!scene->get_grid_remesh_flag()) {
      return;
    }
    _pending_grid_mesh =
      std::make_unique<Grid_mesh>(Grid_mesh_create_info{
        .graphics = &_graphics,
        .grid = &scene->get_grid(),
      });
    scene->clear_grid_remesh_flag();
  }

  std::pair<rc::Strong<graphics::Image>, bool>
  get_depth_image(Eigen::Vector3i const &extent) {
    auto const create_image =
      !_depth_image || _depth_image->get_extent() != extent;
    if (create_image) {
      _depth_image = _graphics.create_image({
        .dimensionality = 2,
        .format = graphics::Image_format::d32_sfloat,
        .extent = extent,
        .mip_level_count = 1,
        .array_layer_count = 1,
        .usage = graphics::Image_usage_flag_bits::depth_attachment,
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
    auto work = _graphics.submit_transient_work(std::move(work_recorder));
    work->await();
    return index_buffer;
  }

  std::tuple<rc::Strong<graphics::Pipeline>, rc::Strong<graphics::Pipeline>>
  get_graphics_pipelines(graphics::Image_format swapchain_image_format) {
    ZoneScoped;
    if (
      !_pipelines_color_format ||
      *_pipelines_color_format != swapchain_image_format) {
      _grid_pipeline = make_grid_pipeline(swapchain_image_format);
      _mesh_pipeline = make_mesh_pipeline(swapchain_image_format);
      _pipelines_color_format = swapchain_image_format;
    }
    return {_grid_pipeline, _mesh_pipeline};
  }

  rc::Strong<graphics::Pipeline>
  make_grid_pipeline(graphics::Image_format swapchain_image_format) {
    auto const push_constant_ranges = std::array{
      graphics::Push_constant_range{
        .stage_flags = graphics::Shader_stage_flag_bits::vertex,
        .offset = 0,
        .size = 88,
      },
      graphics::Push_constant_range{
        .stage_flags = graphics::Shader_stage_flag_bits::fragment,
        .offset = 64,
        .size = 16,
      },
    };
    auto const pipeline_layout =
      _grid_pipeline ? _grid_pipeline->get_layout()
                     : _graphics.create_pipeline_layout({
                         .push_constant_ranges = push_constant_ranges,
                       });
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
    auto const color_attachment_format = swapchain_image_format;
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
      .layout = pipeline_layout,
    });
  }

  rc::Strong<graphics::Pipeline>
  make_mesh_pipeline(graphics::Image_format swapchain_image_format) {
    auto const push_constant_range = graphics::Push_constant_range{
      .stage_flags = graphics::Shader_stage_flag_bits::vertex,
      .offset = 0,
      .size = 72,
    };
    auto const pipeline_layout =
      _mesh_pipeline ? _mesh_pipeline->get_layout()
                     : _graphics.create_pipeline_layout({
                         .push_constant_ranges = {&push_constant_range, 1},
                       });
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
    auto const color_attachment_format = swapchain_image_format;
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
      .layout = pipeline_layout,
    });
  }

  State _state{State::initial};
  Client _client;
  enet::Address _server_address;
  glfw::Unique_window _glfw_window{};
  vk::UniqueSurfaceKHR _vk_surface{};
  graphics::Graphics _graphics{};
  rc::Strong<graphics::Image> _depth_image{};
  graphics::Shader _grid_vertex_shader;
  graphics::Shader _grid_fragment_shader;
  graphics::Shader _mesh_vertex_shader;
  graphics::Shader _mesh_fragment_shader;
  rc::Strong<graphics::Pipeline> _grid_pipeline{};
  rc::Strong<graphics::Pipeline> _mesh_pipeline{};
  std::optional<graphics::Image_format> _pipelines_color_format{};
  std::unique_ptr<Grid_mesh> _grid_mesh;
  std::unique_ptr<Grid_mesh> _pending_grid_mesh;
  rc::Strong<graphics::Buffer> _cube_vertex_buffer{};
  rc::Strong<graphics::Buffer> _cube_index_buffer{};
};

Application::Application(Application_create_info const &create_info)
    : _impl{std::make_unique<Impl>(create_info)} {}

Application::~Application() = default;

bool Application::update(float duration) { return _impl->update(duration); }
} // namespace fpsparty::client
