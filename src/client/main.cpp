#include "client/depth_image_recycler.hpp"
#include "client/frame_counter.hpp"
#include "client/grid_mesh.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "game/client/client.hpp"
#include "game/core/constants.hpp"
#include "glfw.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/graphics.hpp"
#include "graphics/index_buffer.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/shader_stage.hpp"
#include "graphics/synchronization_scope.hpp"
#include "graphics/vertex_buffer.hpp"
#include "graphics/work_done_callback.hpp"
#include "math/transformation_matrices.hpp"
#include "net/core/constants.hpp"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <numbers>
#include <optional>
#include <span>
#include <volk.h>
#include <vulkan/vulkan.hpp>

using namespace fpsparty;

namespace vk {
DispatchLoaderDynamic defaultDispatchLoaderDynamic;
}

namespace {
constexpr auto server_ip = "127.0.0.1";

volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }

vk::UniqueSurfaceKHR make_vk_surface(glfw::Window window) {
  auto retval = glfw::create_window_surface_unique(
      graphics::Global_vulkan_state::get().instance(), window);
  std::cout << "Created VkSurfaceKHR.\n";
  return retval;
}

struct Vertex {
  float x;
  float y;
  float z;
  float r;
  float g;
  float b;
};

const auto floor_mesh_vertices = std::vector<Vertex>{
    {.x = 10.0f, .y = 0.0f, .z = 10.0f, .r = 1.0f, .g = 0.0f, .b = 0.0f},
    {.x = 10.0f, .y = 0.0f, .z = -10.0f, .r = 0.0f, .g = 0.0f, .b = 1.0f},
    {.x = -10.0f, .y = 0.0f, .z = 10.0f, .r = 0.0f, .g = 1.0f, .b = 0.0f},
    {.x = -10.0f, .y = 0.0f, .z = -10.0f, .r = 1.0f, .g = 1.0f, .b = 0.0f},
};

const auto floor_mesh_indices = std::vector<std::uint16_t>{0, 1, 2, 3, 2, 1};

const auto cube_mesh_vertices = std::vector<Vertex>{
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

const auto cube_mesh_indices = std::vector<std::uint16_t>{
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

class Client : public game::Client,
               glfw::Key_callback,
               glfw::Mouse_button_callback,
               glfw::Cursor_pos_callback {
public:
  struct Create_info {
    game::Client_create_info client_info;
    glfw::Window glfw_window;
    vk::SurfaceKHR vk_surface;
  };

  explicit Client(const Create_info &create_info)
      : game::Client{create_info.client_info},
        _glfw_window{create_info.glfw_window},
        _vk_surface{create_info.vk_surface},
        _graphics{{
            .window = _glfw_window,
            .surface = _vk_surface,
            .vsync_preferred = false,
        }},
        _depth_images{
            graphics::recycler_predicates::Image_extent{},
            client::Depth_image_factory{&_graphics},
        },
        _grid_vertex_shader{
            graphics::load_shader("./assets/shaders/grid.vert.spv")},
        _grid_fragment_shader{
            graphics::load_shader("./assets/shaders/grid.frag.spv")},
        _mesh_vertex_shader{
            graphics::load_shader("./assets/shaders/shader.vert.spv")},
        _mesh_fragment_shader{
            graphics::load_shader("./assets/shaders/shader.frag.spv")} {
    // _graphics.set_vsync_preferred(false);
    _glfw_window.set_key_callback(this);
    _glfw_window.set_mouse_button_callback(this);
    _glfw_window.set_cursor_pos_callback(this);
    _cube_vertex_buffer =
        upload_vertices(std::as_bytes(std::span{cube_mesh_vertices}));
    std::cout << "Uploaded cube vertex buffer.\n";
    _cube_index_buffer =
        upload_indices(std::as_bytes(std::span{cube_mesh_indices}));
    std::cout << "Uploaded cube index buffer.\n";
  }

  void render() {
    _frame_counter.notify();
    _graphics.poll_works();
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
    auto depth_image =
        _depth_images.pop(graphics::recycler_predicates::Image_extent{
            swapchain_image->get_extent(),
        });
    work_recorder.begin_rendering({
        .color_image = swapchain_image,
        .depth_image = depth_image,
    });
    const auto [grid_pipeline, mesh_pipeline] =
        get_graphics_pipelines(swapchain_image->get_format());
    work_recorder.set_viewport(swapchain_image->get_extent().head<2>());
    work_recorder.set_scissor(swapchain_image->get_extent().head<2>());
    work_recorder.set_depth_test_enabled(true);
    work_recorder.set_depth_write_enabled(true);
    work_recorder.set_depth_compare_op(graphics::Compare_op::greater);
    const auto game = get_game();
    const auto player = game ? get_player() : nullptr;
    const auto player_humanoid = player ? player->get_humanoid() : nullptr;
    if (player_humanoid) {
      const auto view_matrix =
          (math::x_rotation_matrix(-player_humanoid->get_input_state().pitch) *
           math::y_rotation_matrix(-player_humanoid->get_input_state().yaw) *
           math::translation_matrix(-(player_humanoid->get_position() +
                                      Eigen::Vector3f::UnitY() * 1.7f)))
              .eval();
      const auto aspect_ratio =
          static_cast<float>(swapchain_image->get_extent().x()) /
          static_cast<float>(swapchain_image->get_extent().y());
      const auto projection_matrix = math::perspective_projection_matrix(
          aspect_ratio > 1.0f ? 1.0f : aspect_ratio,
          aspect_ratio > 1.0f ? 1.0f / aspect_ratio : 1.0f,
          0.1f);
      const auto view_projection_matrix =
          (projection_matrix * view_matrix).eval();
      // draw grid
      if (_grid_mesh->is_uploaded()) {
        work_recorder.bind_pipeline(grid_pipeline);
        work_recorder.set_cull_mode(graphics::Cull_mode::none);
        work_recorder.bind_vertex_buffer(_grid_mesh->get_vertex_buffer());
        work_recorder.bind_index_buffer(_grid_mesh->get_index_buffer(),
                                        graphics::Index_type::u32);
        work_recorder.push_constants(
            grid_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex,
            0,
            std::as_bytes(std::span{&view_projection_matrix, 1}));
        const auto pos_x_normal = Eigen::Vector4f{
            1.0f,
            0.0f,
            0.0f,
            game::constants::grid_wall_thickness * 0.5f,
        };
        const auto neg_x_normal = Eigen::Vector4f{
            -1.0f,
            0.0f,
            0.0f,
            game::constants::grid_wall_thickness * 0.5f,
        };
        const auto pos_y_normal = Eigen::Vector4f{
            0.0f,
            1.0f,
            0.0f,
            game::constants::grid_wall_thickness * 0.5f,
        };
        const auto neg_y_normal = Eigen::Vector4f{
            0.0f,
            -1.0f,
            0.0f,
            game::constants::grid_wall_thickness * 0.5f,
        };
        const auto pos_z_normal = Eigen::Vector4f{
            0.0f,
            0.0f,
            1.0f,
            game::constants::grid_wall_thickness * 0.5f,
        };
        const auto neg_z_normal = Eigen::Vector4f{
            0.0f,
            0.0f,
            -1.0f,
            game::constants::grid_wall_thickness * 0.5f,
        };
        work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
        work_recorder.push_constants(
            grid_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex |
                graphics::Shader_stage_flag_bits::fragment,
            64,
            std::as_bytes(std::span{&pos_x_normal, 1}));
        _grid_mesh->record_draw_command(work_recorder, game::Axis::x);
        work_recorder.set_front_face(graphics::Front_face::clockwise);
        work_recorder.push_constants(
            grid_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex |
                graphics::Shader_stage_flag_bits::fragment,
            64,
            std::as_bytes(std::span{&neg_x_normal, 1}));
        _grid_mesh->record_draw_command(work_recorder, game::Axis::x);
        work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
        work_recorder.push_constants(
            grid_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex |
                graphics::Shader_stage_flag_bits::fragment,
            64,
            std::as_bytes(std::span{&pos_y_normal, 1}));
        _grid_mesh->record_draw_command(work_recorder, game::Axis::y);
        work_recorder.set_front_face(graphics::Front_face::clockwise);
        work_recorder.push_constants(
            grid_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex |
                graphics::Shader_stage_flag_bits::fragment,
            64,
            std::as_bytes(std::span{&neg_y_normal, 1}));
        _grid_mesh->record_draw_command(work_recorder, game::Axis::y);
        work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
        work_recorder.push_constants(
            grid_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex |
                graphics::Shader_stage_flag_bits::fragment,
            64,
            std::as_bytes(std::span{&pos_z_normal, 1}));
        _grid_mesh->record_draw_command(work_recorder, game::Axis::z);
        work_recorder.set_front_face(graphics::Front_face::clockwise);
        work_recorder.push_constants(
            grid_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex |
                graphics::Shader_stage_flag_bits::fragment,
            64,
            std::as_bytes(std::span{&neg_z_normal, 1}));
        _grid_mesh->record_draw_command(work_recorder, game::Axis::z);
      }
      /*
      // draw floor
      work_recorder.bind_vertex_buffer(_floor_vertex_buffer);
      work_recorder.bind_index_buffer(_floor_index_buffer,
                                      graphics::Index_type::u16);
      work_recorder.push_constants(
          _mesh_pipeline->get_layout(),
          graphics::Shader_stage_flag_bits::vertex,
          0,
          std::as_bytes(std::span{&view_projection_matrix, 1}));
      work_recorder.draw_indexed(floor_mesh_indices.size());
      */
      // draw cubes
      work_recorder.bind_pipeline(mesh_pipeline);
      work_recorder.set_cull_mode(graphics::Cull_mode::back);
      work_recorder.set_front_face(graphics::Front_face::counter_clockwise);
      work_recorder.bind_vertex_buffer(_cube_vertex_buffer);
      work_recorder.bind_index_buffer(_cube_index_buffer,
                                      graphics::Index_type::u16);
      // draw other players (cubes)
      for (const auto &other_humanoid :
           game->get_entities()
               .get_entities_of_type<game::Replicated_humanoid>()) {
        if (other_humanoid != player_humanoid) {
          const auto model_matrix =
              (math::translation_matrix(other_humanoid->get_position() +
                                        Eigen::Vector3f::UnitY() * 0.9f) *
               math::y_rotation_matrix(other_humanoid->get_input_state().yaw) *
               math::axis_aligned_scale_matrix({0.7f, 1.8f, 0.7f}))
                  .eval();
          const auto model_view_projection_matrix =
              (view_projection_matrix * model_matrix).eval();
          work_recorder.push_constants(
              _mesh_pipeline->get_layout(),
              graphics::Shader_stage_flag_bits::vertex,
              0,
              std::as_bytes(std::span{&model_view_projection_matrix, 1}));
          work_recorder.draw_indexed({
              .index_count =
                  static_cast<std::uint32_t>(cube_mesh_indices.size()),
          });
        }
      }
      // draw projectiles (cubes)
      for (const auto &projectile :
           game->get_entities()
               .get_entities_of_type<game::Replicated_projectile>()) {
        const auto model_matrix =
            (math::translation_matrix(projectile->get_position()) *
             math::uniform_scale_matrix(0.25f))
                .eval();
        const auto model_view_projection_matrix =
            (view_projection_matrix * model_matrix).eval();
        work_recorder.push_constants(
            _mesh_pipeline->get_layout(),
            graphics::Shader_stage_flag_bits::vertex,
            0,
            std::as_bytes(std::span{&model_view_projection_matrix, 1}));
        work_recorder.draw_indexed({
            .index_count = static_cast<std::uint32_t>(cube_mesh_indices.size()),
        });
      }
    }
    work_recorder.end_rendering();
    work_recorder.transition_image_layout(
        {
            .stage_mask =
                graphics::Pipeline_stage_flag_bits::color_attachment_output,
            .access_mask = graphics::Access_flag_bits::color_attachment_write,
        },
        {},
        graphics::Image_layout::general,
        graphics::Image_layout::present_src,
        swapchain_image);
    auto frame_work = _graphics.submit_frame_work(std::move(work_recorder));
    _depth_images.push(std::move(depth_image), std::move(frame_work));
  }

  constexpr glfw::Window get_window() const noexcept { return _glfw_window; }

protected:
  void on_update_grid() override {
    std::cout << get_game()->get_grid().get_width() << " width\n";
    std::cout << get_game()->get_grid().get_height() << " height\n";
    std::cout << get_game()->get_grid().get_depth() << " depth\n";
    _grid_mesh =
        std::make_unique<client::Grid_mesh>(client::Grid_mesh_create_info{
            .graphics = &_graphics,
            .grid = &get_game()->get_grid(),
        });
  }

  void on_key(glfw::Window,
              glfw::Key key,
              int,
              glfw::Press_action action,
              int) override {
    if (key == glfw::Key::k_escape && action == glfw::Press_action::press) {
      _glfw_window.set_cursor_input_mode(glfw::Cursor_input_mode::normal);
    } else if (has_game_state()) {
      if (const auto player = get_player()) {
        auto input_state = player->get_input_state();
        switch (key) {
        case glfw::Key::k_w:
          input_state.move_forward = action != glfw::Press_action::release;
          break;
        case glfw::Key::k_a:
          input_state.move_left = action != glfw::Press_action::release;
          break;
        case glfw::Key::k_s:
          input_state.move_backward = action != glfw::Press_action::release;
          break;
        case glfw::Key::k_d:
          input_state.move_right = action != glfw::Press_action::release;
          break;
        default:
        }
        player->set_input_state(input_state);
      }
    }
  }

  void on_mouse_button(glfw::Window,
                       glfw::Mouse_button button,
                       glfw::Press_state action,
                       int) override {
    if (button == glfw::Mouse_button::mb_right &&
        action == glfw::Press_state::press) {
      _glfw_window.set_cursor_input_mode(glfw::Cursor_input_mode::disabled);
    } else if (has_game_state()) {
      if (const auto player = get_player()) {
        auto input_state = player->get_input_state();
        if (button == glfw::Mouse_button::mb_left) {
          input_state.use_primary = action != glfw::Press_state::release;
        }
        player->set_input_state(input_state);
      }
    }
  }

  void on_cursor_pos(
      glfw::Window, double, double, double dxpos, double dypos) override {
    if (const auto player = has_game_state() ? get_player() : nullptr) {
      if (_glfw_window.get_cursor_input_mode() ==
          glfw::Cursor_input_mode::disabled) {
        auto input_state = player->get_input_state();
        input_state.yaw -=
            static_cast<float>(dxpos * constants::mouselook_sensititvity);
        input_state.pitch +=
            static_cast<float>(dypos * constants::mouselook_sensititvity);
        input_state.pitch = std::clamp(input_state.pitch,
                                       -0.5f * std::numbers::pi_v<float>,
                                       0.5f * std::numbers::pi_v<float>);
        player->set_input_state(input_state);
      }
    }
  }

private:
  rc::Strong<graphics::Vertex_buffer>
  upload_vertices(std::span<const std::byte> data) {
    const auto staging_buffer = _graphics.create_staging_buffer(data);
    auto vertex_buffer = _graphics.create_vertex_buffer(data.size());
    auto work_recorder = _graphics.record_transient_work();
    work_recorder.copy_buffer(staging_buffer,
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

  rc::Strong<graphics::Index_buffer>
  upload_indices(std::span<const std::byte> data) {
    const auto staging_buffer = _graphics.create_staging_buffer(data);
    auto index_buffer = _graphics.create_index_buffer(data.size());
    auto work_recorder = _graphics.record_transient_work();
    work_recorder.copy_buffer(staging_buffer,
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
    if (!_pipelines_color_format ||
        *_pipelines_color_format != swapchain_image_format) {
      _grid_pipeline = make_grid_pipeline(swapchain_image_format);
      _mesh_pipeline = make_mesh_pipeline(swapchain_image_format);
    }
    return {_grid_pipeline, _mesh_pipeline};
  }

  rc::Strong<graphics::Pipeline>
  make_grid_pipeline(graphics::Image_format swapchain_image_format) {
    const auto push_constant_ranges = std::array{
        graphics::Push_constant_range{
            .stage_flags = graphics::Shader_stage_flag_bits::vertex,
            .offset = 0,
            .size = 80,
        },
        graphics::Push_constant_range{
            .stage_flags = graphics::Shader_stage_flag_bits::fragment,
            .offset = 64,
            .size = 16,
        },
    };
    const auto pipeline_layout =
        _grid_pipeline ? _grid_pipeline->get_layout()
                       : _graphics.create_pipeline_layout({
                             .push_constant_ranges = push_constant_ranges,
                         });
    const auto shader_stages =
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
    const auto vertex_binding = graphics::Vertex_binding_description{
        .binding = 0,
        .stride = 12,
    };
    const auto vertex_attributes = std::array{
        graphics::Vertex_attribute_description{
            .location = 0,
            .binding = 0,
            .format = graphics::Vertex_attribute_format::r32g32b32_sfloat,
            .offset = 0,
        },
    };
    const auto color_attachment_format = swapchain_image_format;
    return _graphics.create_pipeline({
        .shader_stages = std::span{shader_stages},
        .vertex_input_state =
            {
                .bindings = {&vertex_binding, 1},
                .attributes = vertex_attributes,
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
    const auto push_constant_range = graphics::Push_constant_range{
        .stage_flags = graphics::Shader_stage_flag_bits::vertex,
        .offset = 0,
        .size = 64,
    };
    const auto pipeline_layout =
        _mesh_pipeline ? _mesh_pipeline->get_layout()
                       : _graphics.create_pipeline_layout({
                             .push_constant_ranges = {&push_constant_range, 1},
                         });
    const auto shader_stages =
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
    const auto vertex_binding = graphics::Vertex_binding_description{
        .binding = 0,
        .stride = 24,
    };
    const auto vertex_attributes =
        std::vector<graphics::Vertex_attribute_description>{
            {
                .location = 0,
                .binding = 0,
                .format = graphics::Vertex_attribute_format::r32g32b32_sfloat,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = graphics::Vertex_attribute_format::r32g32b32_sfloat,
                .offset = 12,
            }};
    const auto color_attachment_format = swapchain_image_format;
    return _graphics.create_pipeline({
        .shader_stages = std::span{shader_stages},
        .vertex_input_state =
            {
                .bindings = {&vertex_binding, 1},
                .attributes = vertex_attributes,
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

  glfw::Window _glfw_window{};
  vk::SurfaceKHR _vk_surface{};
  client::Frame_counter _frame_counter{};
  graphics::Graphics _graphics{};
  client::Depth_image_recycler _depth_images;
  graphics::Shader _grid_vertex_shader;
  graphics::Shader _grid_fragment_shader;
  graphics::Shader _mesh_vertex_shader;
  graphics::Shader _mesh_fragment_shader;
  rc::Strong<graphics::Pipeline> _grid_pipeline{};
  rc::Strong<graphics::Pipeline> _mesh_pipeline{};
  std::optional<graphics::Image_format> _pipelines_color_format{};
  std::unique_ptr<client::Grid_mesh> _grid_mesh;
  rc::Strong<graphics::Vertex_buffer> _cube_vertex_buffer{};
  rc::Strong<graphics::Index_buffer> _cube_index_buffer{};
};

} // namespace

int main() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto enet_guard = enet::Initialization_guard{{}};
  const auto glfw_guard = glfw::Initialization_guard{{}};
  const auto glfw_window = glfw::create_window_unique({
      .width = 1024,
      .height = 768,
      .title = "FPS Party",
      .resizable = true,
      .client_api = glfw::Client_api::no_api,
  });
  std::cout << "Opened window.\n";
  const auto vulkan_guard = graphics::Global_vulkan_state_guard{{}};
  const auto vk_surface = make_vk_surface(*glfw_window);
  auto client = Client{{
      .client_info = {},
      .glfw_window = *glfw_window,
      .vk_surface = *vk_surface,
  }};
  client.connect({
      .host = *enet::parse_ip(server_ip),
      .port = net::constants::port,
  });
  std::cout << "Connecting to " << server_ip << " on port "
            << net::constants::port << ".\n";
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;
  auto loop_duration = Duration{};
  auto loop_time = Clock::now();
  while (!signal_status && !client.get_window().should_close()) {
    client.poll_events();
    glfw::poll_events();
    if (client.has_game_state()) {
      client.service_game_state(
          std::chrono::duration_cast<std::chrono::duration<float>>(
              loop_duration)
              .count());
    } else if (!client.is_connecting() && !client.is_connected()) {
      break;
    }
    client.render();
    const auto now = Clock::now();
    loop_duration = now - loop_time;
    loop_time = now;
  }
  graphics::Global_vulkan_state::get().device().waitIdle();
  std::cout << "Exiting.\n";
  return 0;
}
