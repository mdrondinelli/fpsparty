#ifndef FPSPARTY_RENDER_GRAPH_ACCESS_HPP
#define FPSPARTY_RENDER_GRAPH_ACCESS_HPP

#include "graphics/synchronization_scope.hpp"

namespace fpsparty::render_graph {

using Access = graphics::Synchronization_scope;

namespace access {

constexpr Access compute_sampled_read{
  .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
  .access_mask = graphics::Access_flag_bits::shader_sampled_read,
};

constexpr Access compute_storage_read{
  .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
  .access_mask = graphics::Access_flag_bits::shader_storage_read,
};

constexpr Access compute_storage_write{
  .stage_mask = graphics::Pipeline_stage_flag_bits::compute_shader,
  .access_mask = graphics::Access_flag_bits::shader_storage_write,
};

constexpr Access fragment_sampled_read{
  .stage_mask = graphics::Pipeline_stage_flag_bits::fragment_shader,
  .access_mask = graphics::Access_flag_bits::shader_sampled_read,
};

constexpr Access color_attachment_write{
  .stage_mask = graphics::Pipeline_stage_flag_bits::color_attachment_output,
  .access_mask = graphics::Access_flag_bits::color_attachment_write,
};

} // namespace access
} // namespace fpsparty::render_graph

#endif
