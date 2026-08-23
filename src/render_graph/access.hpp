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

constexpr Access indirect_command_read{
  .stage_mask = graphics::Pipeline_stage_flag_bits::draw_indirect,
  .access_mask = graphics::Access_flag_bits::indirect_command_read,
};

constexpr Access transfer_write{
  .stage_mask = graphics::Pipeline_stage_flag_bits::transfer,
  .access_mask = graphics::Access_flag_bits::transfer_write,
};

constexpr Access depth_attachment_write{
  .stage_mask = graphics::Pipeline_stage_flag_bits::early_fragment_tests |
                graphics::Pipeline_stage_flag_bits::late_fragment_tests,
  .access_mask = graphics::Access_flag_bits::depth_stencil_attachment_read |
                 graphics::Access_flag_bits::depth_stencil_attachment_write,
};

} // namespace access
} // namespace fpsparty::render_graph

#endif
