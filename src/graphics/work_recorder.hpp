#ifndef FPSPARTY_GRAPHICS_COMMAND_LIST_HPP
#define FPSPARTY_GRAPHICS_COMMAND_LIST_HPP

#include "graphics/buffer.hpp"
#include "graphics/compare_op.hpp"
#include "graphics/cull_mode.hpp"
#include "graphics/front_face.hpp"
#include "graphics/image.hpp"
#include "graphics/image_layout.hpp"
#include "graphics/index_type.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/pipeline_layout.hpp"
#include "graphics/synchronization_scope.hpp"
#include "graphics/work_resource.hpp"
#include "rc.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Work_recorder;

namespace detail {
Work_recorder acquire_work_recorder(Work_resource resource) noexcept;

Work_resource release_work_recorder(Work_recorder recorder) noexcept;
} // namespace detail

struct Buffer_copy_info {
  std::size_t src_offset;
  std::size_t dst_offset;
  std::size_t size;
};

struct Rendering_begin_info {
  rc::Strong<Image> color_image;
  rc::Strong<Image> depth_image{};
};

struct Indexed_draw_info {
  std::uint32_t index_count;
  std::uint32_t instance_count;
  std::uint32_t first_index;
  std::int32_t vertex_offset;
  std::uint32_t first_instance;
};

struct Indirect_indexed_draw_info {
  rc::Strong<Buffer> buffer;
  std::uint64_t offset;
  std::uint32_t draw_count;
  std::uint32_t stride;
};

class Work_recorder {
public:
  void copy_buffer(
    rc::Strong<Buffer const> src,
    rc::Strong<Buffer> dst,
    Buffer_copy_info const &info);

  void barrier(
    Synchronization_scope const &src_scope,
    Synchronization_scope const &dst_scope);

  void transition_image_layout(
    Synchronization_scope const &src_scope,
    Synchronization_scope const &dst_scope,
    Image_layout old_layout,
    Image_layout new_layout,
    rc::Strong<Image> image);

  void begin_rendering(Rendering_begin_info const &info);

  void end_rendering();

  void bind_pipeline(rc::Strong<Pipeline const> pipeline);

  void set_cull_mode(Cull_mode cull_mode);

  void set_front_face(Front_face front_face);

  void set_viewport(Eigen::Vector2i const &extent);

  void set_scissor(Eigen::Vector2i const &extent);

  void set_depth_test_enabled(bool enabled);

  void set_depth_write_enabled(bool enabled);

  void set_depth_compare_op(Compare_op op);

  void bind_vertex_buffer(rc::Strong<Buffer const> buffer);

  void
  bind_index_buffer(rc::Strong<Buffer const> buffer, Index_type index_type);

  void draw_indexed(Indexed_draw_info const &info) noexcept;

  void draw_indexed_indirect(Indirect_indexed_draw_info const &info) noexcept;

  void push_constants(
    rc::Strong<Pipeline_layout const> pipeline_layout,
    Shader_stage_flags stage_flags,
    std::uint32_t offset,
    std::span<std::byte const> data) noexcept;

  void add_reference(rc::Strong<Buffer const> buffer);

  void add_reference(rc::Strong<Image const> image);

  void add_reference(rc::Strong<Pipeline const> pipeline_layout);

  void add_reference(rc::Strong<Pipeline_layout const> pipeline_layout);

private:
  friend Work_recorder
  detail::acquire_work_recorder(detail::Work_resource resource) noexcept;

  friend detail::Work_resource
  detail::release_work_recorder(Work_recorder recorder) noexcept;

  explicit Work_recorder(detail::Work_resource resource) noexcept;

  vk::CommandBuffer get_command_buffer() const noexcept;

  detail::Work_resource _resource;
};
} // namespace fpsparty::graphics

#endif
