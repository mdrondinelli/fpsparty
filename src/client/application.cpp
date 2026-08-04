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
#include <render_graph/graph.hpp>

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
#include "passes/distant_irradiance_passes.hpp"
#include "passes/composite_pass.hpp"
#include "passes/crosshair_pass.hpp"
#include "passes/gbuffer_pass.hpp"
#include "passes/radiance_pass.hpp"
#include "passes/rt_entity_binning_pass.hpp"
#include "passes/sky_irradiance_pass.hpp"
#include "passes/sky_view_pass.hpp"
#include "rt_entity.hpp"
#include "rt_math.hpp"
#include "scene_uniform_layout.hpp"
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
auto constexpr z_near = 0.1f;
auto const transmittance_lut_size = math::ivec2{256, 128};
auto const sky_view_lut_size = math::ivec2{256, 256};

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
        _sky_irradiance_compute_shader{graphics::load_shader(
          "./assets/shaders/atmosphere/sky_irradiance.comp.spv")},
        _distant_irradiance_compute_shader{
          graphics::load_shader("./assets/shaders/distant_irradiance.comp.spv")},
        _distant_irradiance_trace_sun_compute_shader{graphics::load_shader(
          "./assets/shaders/distant_irradiance_trace_sun.comp.spv")},
        _distant_irradiance_trace_sky_compute_shader{graphics::load_shader(
          "./assets/shaders/distant_irradiance_trace_sky.comp.spv")},
        _distant_irradiance_variance_compute_shader{graphics::load_shader(
          "./assets/shaders/distant_irradiance_variance.comp.spv")},
        _distant_irradiance_spatial_filter_compute_shader{
          graphics::load_shader(
            "./assets/shaders/distant_irradiance_spatial_filter.comp.spv")},
        _indirect_dispatch_args_compute_shader{graphics::load_shader(
          "./assets/shaders/indirect_dispatch_args.comp.spv")},
        _rt_grid_entity_binning_compute_shader{graphics::load_shader(
          "./assets/shaders/bin_rt_grid_entities.comp.spv")},
        _radiance_compute_shader{
          graphics::load_shader("./assets/shaders/radiance.comp.spv")},
        _grid_pipeline{make_grid_pipeline()},
        _mesh_pipeline{make_mesh_pipeline()},
        _crosshair_pipeline{make_crosshair_pipeline()},
        _sky_view_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_sky_view_compute_shader})},
        _sky_irradiance_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_sky_irradiance_compute_shader})},
        _distant_irradiance_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_distant_irradiance_compute_shader})},
        _distant_irradiance_trace_sun_pipeline{
          _graphics.create_compute_pipeline(
            {.shader = &_distant_irradiance_trace_sun_compute_shader})},
        _distant_irradiance_trace_sky_pipeline{
          _graphics.create_compute_pipeline(
            {.shader = &_distant_irradiance_trace_sky_compute_shader})},
        _distant_irradiance_variance_pipeline{
          _graphics.create_compute_pipeline(
            {.shader = &_distant_irradiance_variance_compute_shader})},
        _distant_irradiance_spatial_filter_pipeline{
          _graphics.create_compute_pipeline(
            {.shader = &_distant_irradiance_spatial_filter_compute_shader})},
        _indirect_dispatch_args_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_indirect_dispatch_args_compute_shader})},
        _rt_grid_entity_binning_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_rt_grid_entity_binning_compute_shader})},
        _radiance_pipeline{_graphics.create_compute_pipeline(
          {.shader = &_radiance_compute_shader})},
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
      graphics::Image_format::r16g16b16a16_sfloat,
      framebuffer_extent);
    for (auto i = std::size_t{}; i != 2; ++i) {
      get_color_render_target(
        work_recorder,
        _normal_render_targets[i],
        graphics::Image_format::r16g16_snorm,
        framebuffer_extent);
    }
    get_color_render_target(
      work_recorder,
      _motion_vector_render_target,
      graphics::Image_format::r16g16b16a16_sfloat,
      framebuffer_extent);
    get_color_render_target(
      work_recorder,
      _depth_gradient_render_target,
      graphics::Image_format::r16g16_sfloat,
      framebuffer_extent);
    get_radiance_render_target(work_recorder, framebuffer_extent);
    get_distant_irradiance_render_targets(work_recorder, framebuffer_extent);
    get_distant_irradiance_variance_render_targets(
      work_recorder, framebuffer_extent);
    get_distant_irradiance_filtered_render_targets(
      work_recorder, framebuffer_extent);
    get_distant_light_sample_buffer(framebuffer_extent);
    get_color_render_target(
      work_recorder,
      _crosshair_mask_render_target,
      graphics::Image_format::r8_unorm,
      framebuffer_extent);
    for (auto i = std::size_t{}; i != 2; ++i) {
      get_depth_render_target(work_recorder, _depth_render_targets[i], framebuffer_extent);
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
      _sky_view_pass.update(
        _transmittance_lut,
        _sky_view_lut_symbol,
        camera->position,
        sun->direction,
        sun->irradiance);
      _graph.add_pass(_sky_view_pass);
      _sky_irradiance_pass.update(
        _sky_view_lut_symbol,
        _scene_uniform_buffer_symbol,
        camera->position.y(),
        scene_uniform_offset + scene_sky_irradiance_offset);
      _graph.add_pass(_sky_irradiance_pass);
    }
    _gbuffer_pass.update({
      .albedo_render_target = _albedo_render_target_symbol,
      .normal_render_target = _normal_render_target_symbols[_frame_number % 2],
      .motion_vector_render_target = _motion_vector_render_target_symbol,
      .depth_gradient_render_target = _depth_gradient_render_target_symbol,
      .depth_render_target = _depth_render_target_symbols[_frame_number % 2],
      .framebuffer_size = framebuffer_size,
      .scene_uniform_offset = scene_uniform_offset,
      .client = &_client,
      .local_player = _local_player,
      .scene_uniform_buffer = _scene_uniform_buffer,
      .animation_time = _animation_time,
      .grid_mesh = _grid_mesh.get(),
      .block_texture_registry = &_block_texture_registry,
      .cube_vertex_buffer = _cube_vertex_buffer,
      .cube_index_buffer = _cube_index_buffer,
    });
    _graph.add_pass(_gbuffer_pass);
    {
      auto const &session = _client.get_session();
      if (session && _grid_mesh && _grid_mesh->is_uploaded()) {
        ensure_rt_entity_capacity(std::max(
          session->get_scene().get_current_frame().boxes.size(),
          std::size_t{1}));
      }
    }
    auto const rt_frame = _frame_number % max_frames_in_flight;
    if (_rt_entity_binning_pass.update({
          .client = &_client,
          .grid_mesh = _grid_mesh.get(),
          .entity_buffer = _rt_entity_buffers[rt_frame],
          .binning_buffer = _rt_entity_binning_buffer_symbols[rt_frame],
        })) {
      _graph.add_pass(_rt_entity_binning_pass);
    }
    {
      auto const &session = _client.get_session();
      auto const distant_irradiance_camera =
        session && _local_player && _local_player->player_entity_id &&
            _local_player->humanoid_entity_id
          ? session->get_scene().get_camera(*_local_player->player_entity_id)
          : nullptr;
      auto const distant_irradiance_sun =
        distant_irradiance_camera
          ? session->get_scene().get_distant_light(
              scene::elements::sun_light_key)
          : nullptr;
      if (distant_irradiance_camera && _grid_mesh &&
          _grid_mesh->is_uploaded() && distant_irradiance_sun) {
        // History is only valid if last frame's pass wrote the other
        // ping-ponged slot -- if this pass was skipped last frame (one of
        // the checks above), or the images were just (re)created, treat
        // history as absent rather than stale.
        auto const history_valid = u32{
          _last_distant_irradiance_frame &&
          *_last_distant_irradiance_frame + 1 == _frame_number};
        _last_distant_irradiance_frame = _frame_number;
        auto const frame = _frame_number % max_frames_in_flight;
        auto const layout =
          make_rt_entity_binning_buffer_layout(_grid_mesh->get_rt_chunk_count());
        _distant_irradiance_pass1.update({
          .normal_render_target = _normal_render_target_symbols[_frame_number % 2],
          .depth_render_target = _depth_render_target_symbols[_frame_number % 2],
          .transmittance_lut = _transmittance_lut,
          .sky_view_lut = _sky_view_lut_symbol,
          .scene_uniform_buffer = _scene_uniform_buffer_symbol,
          .scene_uniform_offset = scene_uniform_offset,
          .sun_sample_buffer = _distant_light_sample_buffer_sun_symbol,
          .sky_sample_buffer = _distant_light_sample_buffer_sky_symbol,
          .framebuffer_size = framebuffer_size,
          .frame_number = _frame_number,
        });
        _graph.add_pass(_distant_irradiance_pass1);
        _distant_irradiance_indirect_args_pass.update({
          .sun_sample_buffer = _distant_light_sample_buffer_sun_symbol,
          .sky_sample_buffer = _distant_light_sample_buffer_sky_symbol,
        });
        _graph.add_pass(_distant_irradiance_indirect_args_pass);
        auto const shared_trace_inputs = passes::Distant_irradiance_trace_pass_inputs{
          .normal_render_target = _normal_render_target_symbols[_frame_number % 2],
          .depth_render_target = _depth_render_target_symbols[_frame_number % 2],
          .distant_irradiance_render_target =
            _distant_irradiance_render_target_symbols[_frame_number % 2],
          .transmittance_lut = _transmittance_lut,
          .scene_uniform_buffer = _scene_uniform_buffer_symbol,
          .scene_uniform_offset = scene_uniform_offset,
          .sample_buffer = {},
          .rt_block_grid_buffer = _grid_mesh->get_rt_block_grid_buffer(),
          .rt_entity_buffer = _rt_entity_buffers[frame],
          .rt_entity_binning_buffer = _rt_entity_binning_buffer_symbols[frame],
          .rt_entity_binning_grid_offset = layout.grid_offset,
          .rt_entity_binning_nodes_offset = layout.nodes_offset,
          .previous_depth_render_target =
            _depth_render_targets[(_frame_number + 1) % 2],
          .previous_distant_irradiance_render_target =
            _distant_irradiance_render_targets[(_frame_number + 1) % 2],
          .motion_vector_render_target = _motion_vector_render_target_symbol,
          .previous_normal_render_target =
            _normal_render_targets[(_frame_number + 1) % 2],
          .history_valid = history_valid,
          .distant_irradiance_luminance_render_target =
            _distant_irradiance_luminance_render_target_symbols
              [_frame_number % 2],
          .previous_distant_irradiance_luminance_render_target =
            _distant_irradiance_luminance_render_targets
              [(_frame_number + 1) % 2],
        };
        auto sun_trace_inputs = shared_trace_inputs;
        sun_trace_inputs.sample_buffer = _distant_light_sample_buffer_sun_symbol;
        _distant_irradiance_trace_sun_pass.update(std::move(sun_trace_inputs));
        _graph.add_pass(_distant_irradiance_trace_sun_pass);
        auto sky_trace_inputs = shared_trace_inputs;
        sky_trace_inputs.sample_buffer = _distant_light_sample_buffer_sky_symbol;
        _distant_irradiance_trace_sky_pass.update(
          std::move(sky_trace_inputs), _sky_view_lut_symbol);
        _graph.add_pass(_distant_irradiance_trace_sky_pass);
        _distant_irradiance_variance_pass.update({
          .depth_render_target = _depth_render_target_symbols[_frame_number % 2],
          .distant_irradiance_luminance_render_target =
            _distant_irradiance_luminance_render_target_symbols
              [_frame_number % 2],
          .distant_irradiance_variance_render_target =
            _distant_irradiance_variance_render_target_symbols[0],
          .framebuffer_size = framebuffer_size,
        });
        _graph.add_pass(_distant_irradiance_variance_pass);
        auto const update_spatial_filter =
          [&](passes::Distant_irradiance_spatial_filter_pass &pass,
              render_graph::Symbolic_image color_in,
              render_graph::Symbolic_image color_out,
              render_graph::Symbolic_image variance_in,
              render_graph::Symbolic_image variance_out) {
            pass.update({
              .depth_render_target =
                _depth_render_target_symbols[_frame_number % 2],
              .normal_render_target =
                _normal_render_target_symbols[_frame_number % 2],
              .depth_gradient_render_target =
                _depth_gradient_render_target_symbol,
              .color_in = color_in,
              .variance_in = variance_in,
              .color_out = color_out,
              .variance_out = variance_out,
              .framebuffer_size = framebuffer_size,
            });
            _graph.add_pass(pass);
          };
        update_spatial_filter(
          _distant_irradiance_spatial_filter_passes[0],
          _distant_irradiance_render_target_symbols[_frame_number % 2],
          _distant_irradiance_filtered_render_target_symbols[0],
          _distant_irradiance_variance_render_target_symbols[0],
          _distant_irradiance_variance_render_target_symbols[1]);
        update_spatial_filter(
          _distant_irradiance_spatial_filter_passes[1],
          _distant_irradiance_filtered_render_target_symbols[0],
          _distant_irradiance_filtered_render_target_symbols[1],
          _distant_irradiance_variance_render_target_symbols[1],
          _distant_irradiance_variance_render_target_symbols[0]);
        update_spatial_filter(
          _distant_irradiance_spatial_filter_passes[2],
          _distant_irradiance_filtered_render_target_symbols[1],
          _distant_irradiance_filtered_render_target_symbols[0],
          _distant_irradiance_variance_render_target_symbols[0],
          _distant_irradiance_variance_render_target_symbols[1]);
        update_spatial_filter(
          _distant_irradiance_spatial_filter_passes[3],
          _distant_irradiance_filtered_render_target_symbols[0],
          _distant_irradiance_filtered_render_target_symbols[1],
          _distant_irradiance_variance_render_target_symbols[1],
          _distant_irradiance_variance_render_target_symbols[0]);
        // 5 iterations total, steps 1/2/4/8/16 -- the standard SVGF
        // configuration, giving an effective 65x65 pixel filter footprint.
        update_spatial_filter(
          _distant_irradiance_spatial_filter_passes[4],
          _distant_irradiance_filtered_render_target_symbols[1],
          _distant_irradiance_filtered_render_target_symbols[0],
          _distant_irradiance_variance_render_target_symbols[0],
          _distant_irradiance_variance_render_target_symbols[1]);
      }
    }
    _radiance_pass.update({
      .client = &_client,
      .local_player = _local_player,
      .grid_mesh = _grid_mesh.get(),
      .radiance_render_target = _radiance_render_target_symbol,
      .albedo_render_target = _albedo_render_target_symbol,
      .depth_render_target = _depth_render_target_symbols[_frame_number % 2],
      .sky_view_lut = _sky_view_lut_symbol,
      .distant_irradiance_filtered =
        _distant_irradiance_filtered_render_target_symbols[0],
      .framebuffer_size = framebuffer_size,
    });
    _graph.add_pass(_radiance_pass);
    _crosshair_pass.update(
      _crosshair_index_buffer,
      _crosshair_mask_render_target_symbol,
      framebuffer_size);
    _graph.add_pass(_crosshair_pass);
    _composite_pass.update({
      .pipeline = get_composite_pipeline(swapchain_image->get_format()),
      .index_buffer = _composite_index_buffer,
      .swapchain_image = _swapchain_image_symbol,
      .radiance_render_target = _radiance_render_target_symbol,
      .crosshair_mask_render_target = _crosshair_mask_render_target_symbol,
      .framebuffer_size = framebuffer_size,
      .frame_number = _frame_number,
    });
    _graph.add_pass(_composite_pass);
    _graph.execute(
      work_recorder,
      {
        {_albedo_render_target_symbol, _albedo_render_target},
        {_normal_render_target_symbols[0], _normal_render_targets[0]},
        {_normal_render_target_symbols[1], _normal_render_targets[1]},
        {_motion_vector_render_target_symbol, _motion_vector_render_target},
        {_depth_gradient_render_target_symbol, _depth_gradient_render_target},
        {_depth_render_target_symbols[0], _depth_render_targets[0]},
        {_depth_render_target_symbols[1], _depth_render_targets[1]},
        {_radiance_render_target_symbol, _radiance_render_target},
        {_crosshair_mask_render_target_symbol, _crosshair_mask_render_target},
        {_swapchain_image_symbol, swapchain_image},
        {_distant_irradiance_render_target_symbols[0],
         _distant_irradiance_render_targets[0]},
        {_distant_irradiance_render_target_symbols[1],
         _distant_irradiance_render_targets[1]},
        {_distant_irradiance_luminance_render_target_symbols[0],
         _distant_irradiance_luminance_render_targets[0]},
        {_distant_irradiance_luminance_render_target_symbols[1],
         _distant_irradiance_luminance_render_targets[1]},
        {_distant_irradiance_variance_render_target_symbols[0],
         _distant_irradiance_variance_render_targets[0]},
        {_distant_irradiance_variance_render_target_symbols[1],
         _distant_irradiance_variance_render_targets[1]},
        {_distant_irradiance_filtered_render_target_symbols[0],
         _distant_irradiance_filtered_render_targets[0]},
        {_distant_irradiance_filtered_render_target_symbols[1],
         _distant_irradiance_filtered_render_targets[1]},
        {_sky_view_lut_symbol, _sky_view_lut},
      },
      {
        {_scene_uniform_buffer_symbol, _scene_uniform_buffer},
        {_distant_light_sample_buffer_sun_symbol, _distant_light_sample_buffer_sun},
        {_distant_light_sample_buffer_sky_symbol, _distant_light_sample_buffer_sky},
        {_rt_entity_binning_buffer_symbols[0], _rt_entity_binning_buffers[0]},
        {_rt_entity_binning_buffer_symbols[1], _rt_entity_binning_buffers[1]},
      });
    _previous_frame_work = _graphics.submit_frame_work(
      std::move(work_recorder), _previous_frame_work);
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
    auto work_recorder = _graphics.record_transient_work();
    work_recorder.transition_image_layout(
      {},
      compute_shader_storage_write_scope,
      graphics::Image_layout::undefined,
      graphics::Image_layout::general,
      _transmittance_lut);
    work_recorder.bind_compute_pipeline(transmittance_pipeline);
    work_recorder.push_descriptors(
      0,
      {{.image = _transmittance_lut,
        .kind = graphics::Descriptor_kind::storage}});
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
    }
  }

  void get_distant_irradiance_render_targets(
    graphics::Work_recorder &work_recorder, math::ivec3 extent) {
    auto const create_images =
      !_distant_irradiance_render_targets[0] ||
      _distant_irradiance_render_targets[0]->get_extent() != extent;
    if (create_images) {
      _last_distant_irradiance_frame.reset();
      for (auto i = std::size_t{}; i != 2; ++i) {
        _distant_irradiance_render_targets[i] = _graphics.create_image({
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
          _distant_irradiance_render_targets[i]);
        _distant_irradiance_luminance_render_targets[i] =
          _graphics.create_image({
            .dimensionality = 2,
            .format = graphics::Image_format::r32g32_sfloat,
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
          _distant_irradiance_luminance_render_targets[i]);
      }
    }
  }

  void get_distant_irradiance_variance_render_targets(
    graphics::Work_recorder &work_recorder, math::ivec3 extent) {
    auto const create_images = !_distant_irradiance_variance_render_targets[0] ||
      _distant_irradiance_variance_render_targets[0]->get_extent() != extent;
    if (create_images) {
      for (auto i = std::size_t{}; i != 2; ++i) {
        _distant_irradiance_variance_render_targets[i] = _graphics.create_image({
          .dimensionality = 2,
          .format = graphics::Image_format::r16_sfloat,
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
          _distant_irradiance_variance_render_targets[i]);
      }
    }
  }

  void get_distant_irradiance_filtered_render_targets(
    graphics::Work_recorder &work_recorder, math::ivec3 extent) {
    auto const create_images = !_distant_irradiance_filtered_render_targets[0] ||
      _distant_irradiance_filtered_render_targets[0]->get_extent() != extent;
    if (create_images) {
      for (auto i = std::size_t{}; i != 2; ++i) {
        _distant_irradiance_filtered_render_targets[i] = _graphics.create_image({
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
          _distant_irradiance_filtered_render_targets[i]);
      }
    }
  }

  // Worst-case-sized buffers for distant_irradiance.comp (pass 1) to append
  // Distant_light_sample records into and distant_irradiance_trace_sun.comp /
  // distant_irradiance_trace_sky.comp (pass 2) to read them back from -- see
  // distant_irradiance_common.glsl. Layout: a 12-byte VkDispatchIndirectCommand
  // {x, y, z} (written every frame by indirect_dispatch_args.comp from the
  // count below, then read by the trace pass's dispatch_indirect) followed
  // by the 4-byte count and the Distant_light_sample array -- i.e. what the
  // shaders see as Distant_light_samples (count + samples[]) starts 12 bytes
  // into the buffer, not at its start. Each buffer is sized for every pixel
  // picking that buffer's technique (12-byte indirect command + 4-byte count
  // header plus one Distant_light_sample -- packed pixel + uv, 8 bytes --
  // per pixel), since pass 1 can send any pixel to either buffer and never
  // produces more than one sample per pixel total; the two buffers' counts
  // can never both hit their individual worst case at once, but each must
  // be able to alone. count is reset to 0 every frame via fill_buffer, not
  // recreated -- no need to fully clear the sample data itself, since pass
  // 2 only ever reads indices below whatever count pass 1 ends up with.
  void get_distant_light_sample_buffer(math::ivec3 extent) {
    auto const create_buffers = !_distant_light_sample_buffer_sun ||
      _distant_light_sample_buffer_extent != extent;
    if (create_buffers) {
      auto const pixel_count =
        static_cast<std::size_t>(extent.x()) *
        static_cast<std::size_t>(extent.y());
      auto const buffer_size = std::size_t{16} + pixel_count * std::size_t{8};
      _distant_light_sample_buffer_sun = _graphics.create_buffer({
        .size = buffer_size,
        .usage = graphics::Buffer_usage_flag_bits::shader_device_address |
                 graphics::Buffer_usage_flag_bits::transfer_dst |
                 graphics::Buffer_usage_flag_bits::indirect_buffer,
        .mapping_mode = graphics::Mapping_mode::none,
        .min_alignment = 4,
      });
      _distant_light_sample_buffer_sky = _graphics.create_buffer({
        .size = buffer_size,
        .usage = graphics::Buffer_usage_flag_bits::shader_device_address |
                 graphics::Buffer_usage_flag_bits::transfer_dst |
                 graphics::Buffer_usage_flag_bits::indirect_buffer,
        .mapping_mode = graphics::Mapping_mode::none,
        .min_alignment = 4,
      });
      _distant_light_sample_buffer_extent = extent;
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
      graphics::Image_format::r16g16_sfloat,
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
      graphics::Image_format::r16g16_sfloat,
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

  static auto constexpr indirect_command_read_scope =
    graphics::Synchronization_scope{
      .stage_mask = graphics::Pipeline_stage_flag_bits::draw_indirect,
      .access_mask = graphics::Access_flag_bits::indirect_command_read,
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
  rc::Strong<graphics::Image> _albedo_render_target{};
  // Ping-pong pair: [_frame_number % 2] is written this frame, the other
  // holds last frame's normals for temporal reprojection.
  std::array<rc::Strong<graphics::Image>, 2> _normal_render_targets{};
  rc::Strong<graphics::Image> _motion_vector_render_target{};
  rc::Strong<graphics::Image> _depth_gradient_render_target{};
  rc::Strong<graphics::Image> _radiance_render_target{};
  // Ping-pong pair: [_frame_number % 2] is written this frame, the other
  // [(_frame_number + 1) % 2] is read as history for temporal accumulation.
  std::array<rc::Strong<graphics::Image>, 2>
    _distant_irradiance_render_targets{};
  // Luminance/luminance^2 moments of the same signal, ping-ponged and
  // gated by _last_distant_irradiance_frame the same as the color target.
  std::array<rc::Strong<graphics::Image>, 2>
    _distant_irradiance_luminance_render_targets{};
  std::optional<u32> _last_distant_irradiance_frame{};
  // Variance of the distant irradiance signal: slot [0] is
  // distant_irradiance_variance.comp's 3x3-blurred seed (derived from the
  // luminance moments above); each a-trous iteration then reads one slot
  // and writes its own propagated variance into the other. Ping-ponged
  // within a frame only (same two images reused every frame), not across
  // frames -- recomputed fresh every frame, not itself temporally
  // accumulated.
  std::array<rc::Strong<graphics::Image>, 2>
    _distant_irradiance_variance_render_targets{};
  // 5x5 a-trous-filtered color, one slot per iteration (iteration 2 reads
  // iteration 1's output, so they can't share an image) -- also recomputed
  // fresh every frame.
  std::array<rc::Strong<graphics::Image>, 2>
    _distant_irradiance_filtered_render_targets{};
  rc::Strong<graphics::Buffer> _distant_light_sample_buffer_sun{};
  rc::Strong<graphics::Buffer> _distant_light_sample_buffer_sky{};
  math::ivec3 _distant_light_sample_buffer_extent{};
  rc::Strong<graphics::Image> _crosshair_mask_render_target{};
  graphics::Shader _grid_vertex_shader;
  graphics::Shader _grid_fragment_shader;
  graphics::Shader _mesh_vertex_shader;
  graphics::Shader _mesh_fragment_shader;
  graphics::Shader _crosshair_vertex_shader;
  graphics::Shader _crosshair_fragment_shader;
  graphics::Shader _composite_vertex_shader;
  graphics::Shader _composite_fragment_shader;
  graphics::Shader _sky_view_compute_shader;
  graphics::Shader _sky_irradiance_compute_shader;
  graphics::Shader _distant_irradiance_compute_shader;
  graphics::Shader _distant_irradiance_trace_sun_compute_shader;
  graphics::Shader _distant_irradiance_trace_sky_compute_shader;
  graphics::Shader _distant_irradiance_variance_compute_shader;
  graphics::Shader _distant_irradiance_spatial_filter_compute_shader;
  graphics::Shader _indirect_dispatch_args_compute_shader;
  graphics::Shader _rt_grid_entity_binning_compute_shader;
  graphics::Shader _radiance_compute_shader;
  rc::Strong<graphics::Pipeline> _grid_pipeline{};
  rc::Strong<graphics::Pipeline> _mesh_pipeline{};
  passes::Gbuffer_pass _gbuffer_pass{
    _grid_pipeline, _mesh_pipeline, cube_mesh_indices.size(), z_near};
  rc::Strong<graphics::Pipeline> _crosshair_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _sky_view_pipeline{};
  rc::Strong<graphics::Compute_pipeline> _sky_irradiance_pipeline{};
  passes::Sky_view_pass _sky_view_pass{_sky_view_pipeline, sky_view_lut_size};
  passes::Sky_irradiance_pass _sky_irradiance_pass{_sky_irradiance_pipeline};
  render_graph::Graph _graph{};
  // Symbolic ids for every render-graph-tracked resource -- see
  // render_graph/symbolic_resource.hpp. Allocated once here; the bulk
  // images/buffers lists passed to _graph.execute() at the end of
  // render() tell Graph what each resolves to this frame. A resource no
  // Node in the graph ever writes (content textures, static geometry
  // buffers, host-only buffers) has no symbol here at all -- see each
  // pass's header for which fields those are.
  std::array<render_graph::Symbolic_image, 2> _depth_render_target_symbols{
    _graph.allocate_image_symbol(), _graph.allocate_image_symbol()};
  render_graph::Symbolic_image _albedo_render_target_symbol{
    _graph.allocate_image_symbol()};
  std::array<render_graph::Symbolic_image, 2> _normal_render_target_symbols{
    _graph.allocate_image_symbol(), _graph.allocate_image_symbol()};
  render_graph::Symbolic_image _motion_vector_render_target_symbol{
    _graph.allocate_image_symbol()};
  render_graph::Symbolic_image _depth_gradient_render_target_symbol{
    _graph.allocate_image_symbol()};
  render_graph::Symbolic_image _radiance_render_target_symbol{
    _graph.allocate_image_symbol()};
  std::array<render_graph::Symbolic_image, 2>
    _distant_irradiance_render_target_symbols{
      _graph.allocate_image_symbol(), _graph.allocate_image_symbol()};
  std::array<render_graph::Symbolic_image, 2>
    _distant_irradiance_luminance_render_target_symbols{
      _graph.allocate_image_symbol(), _graph.allocate_image_symbol()};
  std::array<render_graph::Symbolic_image, 2>
    _distant_irradiance_variance_render_target_symbols{
      _graph.allocate_image_symbol(), _graph.allocate_image_symbol()};
  std::array<render_graph::Symbolic_image, 2>
    _distant_irradiance_filtered_render_target_symbols{
      _graph.allocate_image_symbol(), _graph.allocate_image_symbol()};
  render_graph::Symbolic_image _crosshair_mask_render_target_symbol{
    _graph.allocate_image_symbol()};
  render_graph::Symbolic_image _swapchain_image_symbol{
    _graph.allocate_image_symbol()};
  render_graph::Symbolic_image _sky_view_lut_symbol{
    _graph.allocate_image_symbol()};
  render_graph::Symbolic_buffer _scene_uniform_buffer_symbol{
    _graph.allocate_buffer_symbol()};
  render_graph::Symbolic_buffer _distant_light_sample_buffer_sun_symbol{
    _graph.allocate_buffer_symbol()};
  render_graph::Symbolic_buffer _distant_light_sample_buffer_sky_symbol{
    _graph.allocate_buffer_symbol()};
  std::array<render_graph::Symbolic_buffer, max_frames_in_flight>
    _rt_entity_binning_buffer_symbols{
      _graph.allocate_buffer_symbol(), _graph.allocate_buffer_symbol()};
  rc::Strong<graphics::Compute_pipeline> _distant_irradiance_pipeline{};
  passes::Distant_irradiance_pass1 _distant_irradiance_pass1{
    _distant_irradiance_pipeline};
  rc::Strong<graphics::Compute_pipeline> _distant_irradiance_trace_sun_pipeline{};
  passes::Distant_irradiance_trace_sun_pass _distant_irradiance_trace_sun_pass{
    _distant_irradiance_trace_sun_pipeline};
  rc::Strong<graphics::Compute_pipeline> _distant_irradiance_trace_sky_pipeline{};
  passes::Distant_irradiance_trace_sky_pass _distant_irradiance_trace_sky_pass{
    _distant_irradiance_trace_sky_pipeline};
  rc::Strong<graphics::Compute_pipeline> _distant_irradiance_variance_pipeline{};
  passes::Distant_irradiance_variance_pass _distant_irradiance_variance_pass{
    _distant_irradiance_variance_pipeline};
  rc::Strong<graphics::Compute_pipeline>
    _distant_irradiance_spatial_filter_pipeline{};
  passes::Distant_irradiance_spatial_filter_pass
    _distant_irradiance_spatial_filter_passes[5]{
      {_distant_irradiance_spatial_filter_pipeline, 1u},
      {_distant_irradiance_spatial_filter_pipeline, 2u},
      {_distant_irradiance_spatial_filter_pipeline, 4u},
      {_distant_irradiance_spatial_filter_pipeline, 8u},
      {_distant_irradiance_spatial_filter_pipeline, 16u},
    };
  rc::Strong<graphics::Compute_pipeline> _indirect_dispatch_args_pipeline{};
  passes::Distant_irradiance_indirect_args_pass
    _distant_irradiance_indirect_args_pass{_indirect_dispatch_args_pipeline};
  rc::Strong<graphics::Compute_pipeline> _rt_grid_entity_binning_pipeline{};
  passes::Rt_entity_binning_pass _rt_entity_binning_pass{
    _rt_grid_entity_binning_pipeline};
  rc::Strong<graphics::Compute_pipeline> _radiance_pipeline{};
  passes::Radiance_pass _radiance_pass{_radiance_pipeline};
  rc::Strong<graphics::Pipeline> _composite_pipeline{};
  std::optional<graphics::Image_format> _composite_pipeline_color_format{};
  rc::Strong<graphics::Image> _transmittance_lut{};
  rc::Strong<graphics::Image> _sky_view_lut{};
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
  passes::Crosshair_pass _crosshair_pass{
    _crosshair_pipeline, crosshair_indices.size()};
  rc::Strong<graphics::Buffer> _composite_index_buffer{};
  passes::Composite_pass _composite_pass{composite_indices.size()};
  u32 _frame_number{};
  // Makes this frame's GPU execution wait on last frame's, so a ping-
  // ponged resource's "previous frame" slot is guaranteed visible before
  // this frame reads it -- see Graphics::submit_frame_work.
  rc::Strong<graphics::Work> _previous_frame_work{};
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
