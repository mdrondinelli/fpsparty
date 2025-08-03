#ifndef FPSPARTY_GRAPHICS_GRAPHICS_HPP
#define FPSPARTY_GRAPHICS_GRAPHICS_HPP

#include "glfw.hpp"
#include "graphics/graphics_buffer.hpp"
#include "graphics/index_buffer.hpp"
#include "graphics/staging_buffer.hpp"
#include "graphics/vertex_buffer.hpp"
#include "rc.hpp"
#include "vma.hpp"
#include <Eigen/Dense>
#include <cstddef>
#include <span>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace fpsparty::graphics {
struct Graphics_create_info {
  glfw::Window window{};
  vk::SurfaceKHR surface{};
  unsigned max_frames_in_flight{2};
};

class Graphics_buffer_copy_state
    : public rc::Object<Graphics_buffer_copy_state> {
public:
  explicit Graphics_buffer_copy_state(rc::Strong<Graphics_buffer> src_buffer,
                                      rc::Strong<Graphics_buffer> dst_buffer,
                                      vk::UniqueCommandBuffer command_buffer,
                                      vk::UniqueFence fence);

  void await() const;

  bool is_done() const;

private:
  mutable rc::Strong<Graphics_buffer> _src_buffer;
  mutable rc::Strong<Graphics_buffer> _dst_buffer;
  mutable vk::UniqueCommandBuffer _command_buffer;
  mutable vk::UniqueFence _fence;
  mutable std::mutex _mutex;
};

class Graphics {
public:
  constexpr Graphics() noexcept = default;

  explicit Graphics(const Graphics_create_info &info);

  void collect_garbage();

  std::pair<rc::Strong<Vertex_buffer>, rc::Strong<Graphics_buffer_copy_state>>
  create_vertex_buffer(std::span<const std::byte> data);

  std::pair<rc::Strong<Index_buffer>, rc::Strong<Graphics_buffer_copy_state>>
  create_index_buffer(std::span<const std::byte> data);

  [[nodiscard]] bool begin();

  void end();

  void bind_vertex_buffer(rc::Strong<const Vertex_buffer> buffer);

  void bind_index_buffer(rc::Strong<const Index_buffer> buffer,
                         vk::IndexType index_type);

  void draw_indexed(std::uint32_t index_count) noexcept;

  void
  push_constants(const Eigen::Matrix4f &model_view_projection_matrix) noexcept;

private:
  struct Frame_resource {
    vk::UniqueSemaphore swapchain_image_acquire_semaphore{};
    vk::UniqueSemaphore swapchain_image_release_semaphore{};
    vk::UniqueFence work_done_fence{};
    vk::UniqueCommandPool command_pool{};
    vk::UniqueCommandBuffer command_buffer{};
    vk::UniqueImage depth_image{};
    vma::Unique_allocation depth_allocation{};
    vk::UniqueImageView depth_image_view{};
    std::uint32_t swapchain_image_index{};
    std::vector<rc::Strong<const Graphics_buffer>> used_buffers{};
  };

  vk::UniqueCommandBuffer make_upload_command_buffer();

  void remake_swapchain();

  glfw::Window _window{};
  vk::SurfaceKHR _surface{};
  vk::UniqueSwapchainKHR _swapchain{};
  vk::Format _swapchain_image_format{};
  vk::Extent2D _swapchain_image_extent{};
  std::vector<vk::Image> _swapchain_images{};
  std::vector<vk::UniqueImageView> _swapchain_image_views{};
  vk::UniquePipelineLayout _pipeline_layout{};
  vk::UniquePipeline _pipeline{};
  rc::Factory<Staging_buffer> _staging_buffer_factory{};
  rc::Factory<Vertex_buffer> _vertex_buffer_factory{};
  rc::Factory<Index_buffer> _index_buffer_factory{};
  vk::UniqueCommandPool _copy_command_pool{};
  rc::Factory<Graphics_buffer_copy_state> _buffer_copy_state_factory{};
  std::vector<rc::Strong<Graphics_buffer_copy_state>> _buffer_copy_states{};
  std::vector<Frame_resource> _frame_resources{};
  unsigned _frame_resource_index{};
};
} // namespace fpsparty::graphics

#endif
