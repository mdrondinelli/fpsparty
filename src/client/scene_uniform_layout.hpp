#ifndef FPSPARTY_CLIENT_SCENE_UNIFORM_LAYOUT_HPP
#define FPSPARTY_CLIENT_SCENE_UNIFORM_LAYOUT_HPP

#include <cstddef>

namespace fpsparty::client {

// Must match scene.glsl's Scene struct layout exactly.
auto constexpr scene_uniform_data_size = std::size_t{320};
auto constexpr scene_view_projection_matrix_offset = std::size_t{0};
auto constexpr scene_previous_view_projection_matrix_offset = std::size_t{64};
auto constexpr scene_animation_time_offset = std::size_t{128};
auto constexpr scene_camera_basis_offset = std::size_t{144};
auto constexpr scene_sun_direction_offset = std::size_t{192};
auto constexpr scene_sun_irradiance_offset = std::size_t{208};
auto constexpr scene_zoom_offset = std::size_t{224};
auto constexpr scene_z_near_offset = std::size_t{232};
// 320 = 236 (end of z_near) + 72 (18 floats of sky_irradiance) = 308,
// rounded up to the next 16-byte boundary.
// Must match scene.glsl's sky_irradiance_offset exactly.
auto constexpr scene_sky_irradiance_offset = std::size_t{236};

} // namespace fpsparty::client

#endif
