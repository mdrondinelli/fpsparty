#include "client/global_vulkan_state.hpp"
#include "client/graphics.hpp"
#include "client/index_buffer.hpp"
#include "client/vertex_buffer.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "game/client/client.hpp"
#include "game/client/replicated_humanoid.hpp"
#include "game/client/replicated_player.hpp"
#include "game/client/replicated_projectile.hpp"
#include "glfw.hpp"
#include "math/transformation_matrices.hpp"
#include "net/core/constants.hpp"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <numbers>
#include <span>
#include <volk.h>
#include <vulkan/vulkan.hpp>

using namespace fpsparty;
using namespace fpsparty::client;

namespace vk {
DispatchLoaderDynamic defaultDispatchLoaderDynamic;
}

namespace {
constexpr auto server_ip = "127.0.0.1";

volatile std::sig_atomic_t signal_status{};
void handle_signal(int signal) { signal_status = signal; }

vk::UniqueSurfaceKHR make_vk_surface(glfw::Window window) {
  auto retval = glfw::create_window_surface_unique(
      Global_vulkan_state::get().instance(), window);
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
        }} {
    _glfw_window.set_key_callback(this);
    _glfw_window.set_mouse_button_callback(this);
    _glfw_window.set_cursor_pos_callback(this);
    _floor_vertex_buffer =
        upload_vertices(std::as_bytes(std::span{floor_mesh_vertices}));
    std::cout << "Uploaded floor vertex buffer.\n";
    _floor_index_buffer =
        upload_indices(std::as_bytes(std::span{floor_mesh_indices}));
    std::cout << "Uploaded floor index buffer.\n";
    _cube_vertex_buffer =
        upload_vertices(std::as_bytes(std::span{cube_mesh_vertices}));
    std::cout << "Uploaded cube vertex buffer.\n";
    _cube_index_buffer =
        upload_indices(std::as_bytes(std::span{cube_mesh_indices}));
    std::cout << "Uploaded cube index buffer.\n";
  }

  void render() {
    _graphics.collect_garbage();
    if (_graphics.begin()) {
      const auto game = get_game();
      const auto player = game ? get_player() : nullptr;
      const auto player_humanoid = player ? player->get_humanoid() : nullptr;
      if (player_humanoid) {
        const auto view_matrix =
            (math::x_rotation_matrix(
                 -player_humanoid->get_input_state().pitch) *
             math::y_rotation_matrix(-player_humanoid->get_input_state().yaw) *
             math::translation_matrix(-(player_humanoid->get_position() +
                                        Eigen::Vector3f::UnitY() * 1.7f)))
                .eval();
        const auto framebuffer_size = _glfw_window.get_framebuffer_size();
        const auto framebuffer_aspect =
            static_cast<float>(framebuffer_size[0]) /
            static_cast<float>(framebuffer_size[1]);
        const auto projection_matrix = math::perspective_projection_matrix(
            framebuffer_aspect > 1.0f ? 1.0f : framebuffer_aspect,
            framebuffer_aspect > 1.0f ? 1.0f / framebuffer_aspect : 1.0f,
            0.01f);
        const auto view_projection_matrix =
            (projection_matrix * view_matrix).eval();
        // draw floor
        _graphics.bind_vertex_buffer(_floor_vertex_buffer);
        _graphics.bind_index_buffer(_floor_index_buffer,
                                    vk::IndexType::eUint16);
        _graphics.push_constants(view_projection_matrix);
        _graphics.draw_indexed(floor_mesh_indices.size());
        // draw cubes
        _graphics.bind_vertex_buffer(_cube_vertex_buffer);
        _graphics.bind_index_buffer(_cube_index_buffer, vk::IndexType::eUint16);
        // draw other players (cubes)
        for (const auto &other_humanoid :
             game->get_entities()
                 .get_entities_of_type<game::Replicated_humanoid>()) {
          if (other_humanoid != player_humanoid) {
            const auto model_matrix =
                (math::translation_matrix(other_humanoid->get_position() +
                                          Eigen::Vector3f::UnitY() * 0.9f) *
                 math::y_rotation_matrix(
                     other_humanoid->get_input_state().yaw) *
                 math::axis_aligned_scale_matrix({0.7f, 1.8f, 0.7f}))
                    .eval();
            const auto model_view_projection_matrix =
                (view_projection_matrix * model_matrix).eval();
            _graphics.push_constants(model_view_projection_matrix);
            _graphics.draw_indexed(cube_mesh_indices.size());
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
          _graphics.push_constants(model_view_projection_matrix);
          _graphics.draw_indexed(cube_mesh_indices.size());
        }
      }
      _graphics.end();
    }
  }

  constexpr glfw::Window get_window() const noexcept { return _glfw_window; }

protected:
  void on_key(glfw::Window, glfw::Key key, int, glfw::Press_action action,
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

  void on_mouse_button(glfw::Window, glfw::Mouse_button button,
                       glfw::Press_state action, int) override {
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

  void on_cursor_pos(glfw::Window, double, double, double dxpos,
                     double dypos) override {
    if (const auto player = get_player()) {
      if (_glfw_window.get_cursor_input_mode() ==
          glfw::Cursor_input_mode::disabled) {
        auto input_state = player->get_input_state();
        input_state.yaw -=
            static_cast<float>(dxpos * constants::mouselook_sensititvity);
        input_state.pitch +=
            static_cast<float>(dypos * constants::mouselook_sensititvity);
        input_state.pitch =
            std::clamp(input_state.pitch, -0.5f * std::numbers::pi_v<float>,
                       0.5f * std::numbers::pi_v<float>);
        player->set_input_state(input_state);
      }
    }
  }

private:
  rc::Strong<Vertex_buffer> upload_vertices(std::span<const std::byte> data) {
    auto [buffer, copy] = _graphics.create_vertex_buffer(data);
    copy->await();
    return buffer;
  }

  rc::Strong<Index_buffer> upload_indices(std::span<const std::byte> data) {
    auto [buffer, copy] = _graphics.create_index_buffer(data);
    copy->await();
    return buffer;
  }

  glfw::Window _glfw_window{};
  vk::SurfaceKHR _vk_surface{};
  Graphics _graphics{};
  rc::Strong<Vertex_buffer> _floor_vertex_buffer{};
  rc::Strong<Index_buffer> _floor_index_buffer{};
  rc::Strong<Vertex_buffer> _cube_vertex_buffer{};
  rc::Strong<Index_buffer> _cube_index_buffer{};
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
      .resizable = false,
      .client_api = glfw::Client_api::no_api,
  });
  std::cout << "Opened window.\n";
  const auto vulkan_guard = Global_vulkan_state_guard{{}};
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
  Global_vulkan_state::get().device().waitIdle();
  std::cout << "Exiting.\n";
  return 0;
}
