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
    rc::Strong<const Buffer> src,
    rc::Strong<Buffer> dst,
    const Buffer_copy_info &info);

  void transition_image_layout(
    const Synchronization_scope &src_scope,
    const Synchronization_scope &dst_scope,
    Image_layout old_layout,
    Image_layout new_layout,
    rc::Strong<Image> image);

  void begin_rendering(const Rendering_begin_info &info);

  void end_rendering();

  void bind_pipeline(rc::Strong<const Pipeline> pipeline);

  void set_cull_mode(Cull_mode cull_mode);

  void set_front_face(Front_face front_face);

  void set_viewport(const Eigen::Vector2i &extent);

  void set_scissor(const Eigen::Vector2i &extent);

  void set_depth_test_enabled(bool enabled);

  void set_depth_write_enabled(bool enabled);

  void set_depth_compare_op(Compare_op op);

  void bind_vertex_buffer(rc::Strong<const Buffer> buffer);

  void
  bind_index_buffer(rc::Strong<const Buffer> buffer, Index_type index_type);

  void draw_indexed(const Indexed_draw_info &info) noexcept;

  void draw_indexed_indirect(const Indirect_indexed_draw_info &info) noexcept;

  void push_constants(
    rc::Strong<const Pipeline_layout> pipeline_layout,
    Shader_stage_flags stage_flags,
    std::uint32_t offset,
    std::span<const std::byte> data) noexcept;

private:
  friend Work_recorder
  detail::acquire_work_recorder(detail::Work_resource resource) noexcept;

  friend detail::Work_resource
  detail::release_work_recorder(Work_recorder recorder) noexcept;

  explicit Work_recorder(detail::Work_resource resource) noexcept;

  void add_reference(rc::Strong<const Buffer> buffer);

  void add_reference(rc::Strong<const Image> image);

  void add_reference(rc::Strong<const Pipeline> pipeline_layout);

  void add_reference(rc::Strong<const Pipeline_layout> pipeline_layout);

  vk::CommandBuffer get_command_buffer() const noexcept;

  detail::Work_resource _resource;
};
} // namespace fpsparty::graphics

#endif
