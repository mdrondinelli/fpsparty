#ifndef FPSPARTY_CLIENT_DIRECT_SAMPLE_LAYOUT_HPP
#define FPSPARTY_CLIENT_DIRECT_SAMPLE_LAYOUT_HPP

#include <cstddef>
#include <cstdint>

namespace fpsparty::client {

// Dispatch {x,y,z}, queue count, then one pixel index per entry.
inline constexpr std::size_t direct_sample_count_offset = 12;
inline constexpr std::size_t direct_sample_count_size = 4;
inline constexpr std::size_t direct_sample_data_offset =
  direct_sample_count_offset + direct_sample_count_size;
inline constexpr std::size_t direct_sample_stride = 4;
// Must match local_size_x in direct_trace_brdf.comp and
// direct_trace_sky.comp.
inline constexpr std::uint32_t direct_persistent_workgroup_size = 64;

// Pool size for a device that reports no resident invocation count. Sized
// generously on purpose: a pool smaller than the device leaves hardware
// idle for the whole pass, whereas extra workgroups only pay their own
// launch and exit on finding the cursor drained -- 2048 and 1024 measured
// identical on an RTX 5060 Ti. See persistent_workgroup_count().
inline constexpr std::uint32_t direct_persistent_workgroup_count_fallback =
  2048;

} // namespace fpsparty::client

#endif
