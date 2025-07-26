#include "client/global_vulkan_state.hpp"
#include "client/graphics.hpp"
#include "client/index_buffer.hpp"
#include "client/vertex_buffer.hpp"
#include "constants.hpp"
#include "enet.hpp"
#include "game/client/replicated_game.hpp"
#include "game/client/replicated_humanoid.hpp"
#include "game/client/replicated_player.hpp"
#include "game/client/replicated_projectile.hpp"
#include "glfw.hpp"
#include "math/transformation_matrices.hpp"
#include "net/client.hpp"
#include "net/core/constants.hpp"
#include "net/core/entity_id.hpp"
#include "net/core/sequence_number.hpp"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
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

class Client : net::Client,
               glfw::Key_callback,
               glfw::Mouse_button_callback,
               glfw::Cursor_pos_callback {
public:
  struct Create_info {
    net::Client::Create_info client_info;
    glfw::Window glfw_window;
    vk::SurfaceKHR vk_surface;
  };

  explicit Client(const Create_info &create_info)
      : net::Client{create_info.client_info},
        _glfw_window{create_info.glfw_window},
        _vk_surface{create_info.vk_surface},
        _graphics{{
            .window = _glfw_window,
            .surface = _vk_surface,
        }},
        _game{{}} {
    _glfw_window.set_key_callback(this);
    _glfw_window.set_mouse_button_callback(this);
    _glfw_window.set_cursor_pos_callback(this);
    _vk_command_pool =
        Global_vulkan_state::get().device().createCommandPoolUnique({
            .flags = vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = Global_vulkan_state::get().queue_family_index(),
        });
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
    if (_graphics.begin()) {
      const auto player = get_player();
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
        _graphics.bind_vertex_buffer(_floor_vertex_buffer.get_buffer());
        _graphics.bind_index_buffer(_floor_index_buffer.get_buffer(),
                                    vk::IndexType::eUint16);
        _graphics.push_constants(view_projection_matrix);
        _graphics.draw_indexed(floor_mesh_indices.size());
        // draw cubes
        _graphics.bind_vertex_buffer(_cube_vertex_buffer.get_buffer());
        _graphics.bind_index_buffer(_cube_index_buffer.get_buffer(),
                                    vk::IndexType::eUint16);
        // draw other players (cubes)
        for (const auto &other_humanoid :
             _game.get_world()
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
             _game.get_world()
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

  void service_game_state(float duration) {
    assert(_has_game_state);
    _tick_timer -= duration;
    if (_tick_timer <= 0) {
      _tick_timer += constants::tick_duration;
      const auto player = get_player();
      if (player) {
        auto input_state = player->get_input_state();
        input_state.move_left =
            _glfw_window.get_key(glfw::Key::k_a) == glfw::Press_state::press;
        input_state.move_right =
            _glfw_window.get_key(glfw::Key::k_d) == glfw::Press_state::press;
        input_state.move_forward =
            _glfw_window.get_key(glfw::Key::k_w) == glfw::Press_state::press;
        input_state.move_backward =
            _glfw_window.get_key(glfw::Key::k_s) == glfw::Press_state::press;
        input_state.use_primary =
            _glfw_window.get_mouse_button(glfw::Mouse_button::mb_left) ==
            glfw::Press_state::press;
        net::Client::send_player_input_state(
            *_player_id, _input_sequence_number, input_state);
        _in_flight_input_states.emplace_back(input_state,
                                             _input_sequence_number);
        player->set_input_state(input_state);
        player->set_input_sequence_number(_input_sequence_number);
        ++_input_sequence_number;
      }
      _game.tick(constants::tick_duration);
    }
  }

  void poll_network_events() { net::Client::poll_events(); }

  void wait_network_events(std::uint32_t timeout) {
    net::Client::wait_events(timeout);
  }

  void connect(const enet::Address &address) { net::Client::connect(address); }

  bool is_connecting() const noexcept { return net::Client::is_connecting(); }

  bool is_connected() const noexcept { return net::Client::is_connected(); }

  bool has_game_state() const noexcept { return _has_game_state; }

  game::Replicated_player *get_player() const noexcept {
    return static_cast<game::Replicated_player *>(
        _player_id ? _game.get_world().get_entity_by_id(*_player_id) : nullptr);
  }

  constexpr glfw::Window get_window() const noexcept { return _glfw_window; }

protected:
  void on_connect() override {}

  void on_disconnect() override {
    _player_id = std::nullopt;
    _game.reset();
    _has_game_state = false;
    _tick_timer = 0.0f;
    _input_sequence_number = 0;
    _in_flight_input_states.clear();
  }

  // void on_grid_snapshot(serial::Reader &reader) override {}

  void on_player_join_response(net::Entity_id player_id) override {
    _player_id = player_id;
    std::cout << "Got player join response. id = " << player_id << ".\n";
  }

  void on_entity_snapshot(net::Sequence_number tick_number,
                          serial::Reader &public_state_reader,
                          serial::Reader &player_state_reader) override {
    if (!_has_game_state) {
      send_player_join_request();
    }
    _has_game_state = true;
    _game.load({
        .tick_number = tick_number,
        .public_state_reader = &public_state_reader,
        .player_state_reader = &player_state_reader,
    });
    if (const auto player = get_player()) {
      const auto acknowledged_input_sequence_number =
          player->get_input_sequence_number();
      if (acknowledged_input_sequence_number) {
        while (_in_flight_input_states.size() > 0 &&
               _in_flight_input_states[0].second <=
                   *acknowledged_input_sequence_number) {
          _in_flight_input_states.erase(_in_flight_input_states.begin());
        }
      }
      for (const auto &[input_state, input_sequence_number] :
           _in_flight_input_states) {
        player->set_input_state(input_state);
        player->set_input_sequence_number(input_sequence_number);
        _game.tick(constants::tick_duration);
      }
    }
  }

  void on_key(glfw::Window, glfw::Key key, int, glfw::Press_action action,
              int) override {
    if (key == glfw::Key::k_escape && action == glfw::Press_action::press) {
      _glfw_window.set_cursor_input_mode(glfw::Cursor_input_mode::normal);
    }
  }

  void on_mouse_button(glfw::Window, glfw::Mouse_button button,
                       glfw::Press_state action, int) override {
    if (button == glfw::Mouse_button::mb_left &&
        action == glfw::Press_state::press) {
      _glfw_window.set_cursor_input_mode(glfw::Cursor_input_mode::disabled);
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
  Vertex_buffer upload_vertices(std::span<const std::byte> data) {
    return Vertex_buffer{*_vk_command_pool, data};
  }

  Index_buffer upload_indices(std::span<const std::byte> data) {
    return Index_buffer{*_vk_command_pool, data};
  }

  glfw::Window _glfw_window{};
  vk::SurfaceKHR _vk_surface{};
  Graphics _graphics{};
  vk::UniqueCommandPool _vk_command_pool{};
  Vertex_buffer _floor_vertex_buffer{};
  Index_buffer _floor_index_buffer{};
  Vertex_buffer _cube_vertex_buffer{};
  Index_buffer _cube_index_buffer{};
  bool _has_game_state{};
  game::Replicated_game _game;
  std::optional<std::uint32_t> _player_id{};
  float _tick_timer{};
  net::Sequence_number _input_sequence_number{};
  std::vector<std::pair<net::Input_state, net::Sequence_number>>
      _in_flight_input_states{};
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
    client.poll_network_events();
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
