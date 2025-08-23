#ifndef FPSPARTY_GRAPHICS_COMMAND_LIST_HPP
#define FPSPARTY_GRAPHICS_COMMAND_LIST_HPP

#include "graphics/buffer.hpp"
#include "graphics/cull_mode.hpp"
#include "graphics/front_face.hpp"
#include "graphics/image.hpp"
#include "graphics/image_layout.hpp"
#include "graphics/index_buffer.hpp"
#include "graphics/index_type.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/pipeline_layout.hpp"
#include "graphics/synchronization_scope.hpp"
#include "graphics/vertex_buffer.hpp"
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

class Work_recorder {
public:
  void copy_buffer(rc::Strong<const Buffer> src, rc::Strong<Buffer> dst,
                   const Buffer_copy_info &info);

  void transition_image_layout(const Synchronization_scope &src_scope,
                               const Synchronization_scope &dst_scope,
                               Image_layout old_layout, Image_layout new_layout,
                               rc::Strong<Image> image);

  void begin_rendering(const Rendering_begin_info &info);

  void end_rendering();

  void bind_pipeline(rc::Strong<Pipeline> pipeline);

  void set_cull_mode(Cull_mode cull_mode);

  void set_front_face(Front_face front_face);

  void set_viewport(const Eigen::Vector2i &extent);

  void set_scissor(const Eigen::Vector2i &extent);

  void bind_vertex_buffer(rc::Strong<const Vertex_buffer> buffer);

  void bind_index_buffer(rc::Strong<const Index_buffer> buffer,
                         Index_type index_type);

  void draw_indexed(std::uint32_t index_count) noexcept;

  void push_constants(rc::Strong<const Pipeline_layout> pipeline_layout,
                      Shader_stage_flags stage_flags, std::uint32_t offset,
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
