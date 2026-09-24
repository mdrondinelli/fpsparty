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

} // namespace fpsparty::client

#endif
