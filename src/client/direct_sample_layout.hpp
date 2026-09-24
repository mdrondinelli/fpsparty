#ifndef FPSPARTY_CLIENT_DIRECT_SAMPLE_LAYOUT_HPP
#define FPSPARTY_CLIENT_DIRECT_SAMPLE_LAYOUT_HPP

#include <cstddef>

namespace fpsparty::client {

// Dispatch {x,y,z}, queue count, then one pixel index per entry.
inline constexpr std::size_t direct_sample_count_offset = 12;
inline constexpr std::size_t direct_sample_count_size = 4;
inline constexpr std::size_t direct_sample_data_offset =
  direct_sample_count_offset + direct_sample_count_size;
inline constexpr std::size_t direct_sample_stride = 4;
// Continuation queues: the same {x,y,z,count} header, then one
// Direct_ray_state per suspended ray. Must match direct_common.glsl.
inline constexpr std::size_t direct_ray_state_stride = 64;
inline constexpr std::size_t direct_ray_state_data_offset =
  direct_sample_data_offset;
// How many times the BRDF trace runs before survivors are forced to
// terminate.
inline constexpr std::size_t direct_brdf_trace_pass_count = 3;

} // namespace fpsparty::client

#endif
