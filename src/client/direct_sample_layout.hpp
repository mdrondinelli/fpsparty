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
// Persistent warps for the BRDF trace. Enough to fill the device; extra
// workgroups are harmless, since none waits on another and they simply
// drain the cursor later.
inline constexpr std::size_t direct_persistent_workgroup_count = 2048;

} // namespace fpsparty::client

#endif
