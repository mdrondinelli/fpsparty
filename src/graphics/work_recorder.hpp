#ifndef FPSPARTY_GRAPHICS_COMMAND_LIST_HPP
#define FPSPARTY_GRAPHICS_COMMAND_LIST_HPP

#include "graphics/buffer.hpp"
#include "graphics/compare_op.hpp"
#include "graphics/compute_pipeline.hpp"
#include "graphics/cull_mode.hpp"
#include "graphics/front_face.hpp"
#include "graphics/image.hpp"
#include "graphics/image_layout.hpp"
#include "graphics/index_type.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/synchronization_scope.hpp"
#include "graphics/work_resource.hpp"
#include "math/vec.hpp"
#include "rc.hpp"

#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
class Work_recorder;

namespace detail {

struct Work_recorder_descriptor_info {
  rc::Strong<Buffer> sampler_heap;
  rc::Strong<Buffer> resource_heap;
  std::byte *descriptor_data;
  Descriptor_allocation descriptor_allocation;
};

Work_recorder acquire_work_recorder(
  Work_resource resource,
  std::optional<Work_recorder_descriptor_info> const &descriptor_info) noexcept;

Work_resource release_work_recorder(Work_recorder recorder) noexcept;

} // namespace detail

struct Buffer_copy_info {
  std::size_t src_offset;
  std::size_t dst_offset;
  std::size_t size;
};

struct Buffer_image_copy_info {
  std::size_t src_offset;
  u32 dst_mip_level;
  u32 dst_base_array_layer;
  u32 dst_array_layer_count;
  math::ivec3 dst_offset;
  math::ivec3 dst_extent;
};

struct Rendering_begin_info {
  rc::Strong<Image> color_image;
  rc::Strong<Image> depth_image{};
  math::vec4 color_clear_value{0.0f, 0.0f, 0.0f, 1.0f};
};

struct Indexed_draw_info {
  u32 index_count;
  u32 instance_count;
  u32 first_index;
  i32 vertex_offset;
  u32 first_instance;
};

struct Indirect_indexed_draw_info {
  rc::Strong<Buffer> buffer;
  u64 offset;
  u32 draw_count;
  u32 stride;
};

class Work_recorder {
public:
  u32 upload_sampled_image_descriptor(rc::Strong<Image const> image);

  u32 upload_storage_image_descriptor(rc::Strong<Image> image);

private:
  u32 alloc_descriptor();

public:
  void copy_buffer(
    rc::Strong<Buffer const> src,
    rc::Strong<Buffer> dst,
    Buffer_copy_info const &info);

  void copy_buffer_to_image(
    rc::Strong<Buffer const> src,
    rc::Strong<Image> dst,
    Buffer_image_copy_info const &info);

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

  void bind_compute_pipeline(rc::Strong<Compute_pipeline const> pipeline);

  void set_cull_mode(Cull_mode cull_mode);

  void set_front_face(Front_face front_face);

  void set_viewport(math::ivec2 extent);

  void set_scissor(math::ivec2 extent);

  void set_depth_test_enabled(bool enabled);

  void set_depth_write_enabled(bool enabled);

  void set_depth_compare_op(Compare_op op);

  void
  bind_index_buffer(rc::Strong<Buffer const> buffer, Index_type index_type);

  void draw_indexed(Indexed_draw_info const &info) noexcept;

  void draw_indexed_indirect(Indirect_indexed_draw_info const &info) noexcept;

  void dispatch(u32 x, u32 y, u32 z) noexcept;

  void push_data(u32 push_offset, std::span<std::byte const> data) noexcept;

  void push_buffer_reference(
    u32 push_offset, rc::Strong<Buffer> base, u64 offset = 0) noexcept;

  void add_reference(rc::Strong<Buffer const> buffer);

  void add_reference(rc::Strong<Image const> image);

  void add_reference(rc::Strong<Pipeline const> pipeline);

  void add_reference(rc::Strong<Compute_pipeline const> pipeline);

private:
  friend Work_recorder detail::acquire_work_recorder(
    detail::Work_resource resource,
    std::optional<detail::Work_recorder_descriptor_info> const
      &descriptor_info) noexcept;

  friend detail::Work_resource
  detail::release_work_recorder(Work_recorder recorder) noexcept;

  explicit Work_recorder(detail::Work_resource resource) noexcept;

  vk::CommandBuffer get_command_buffer() const noexcept;

  detail::Work_resource _resource;
  std::byte *_descriptor_data{};
  u32 _descriptor_count{};
};
} // namespace fpsparty::graphics

#endif
