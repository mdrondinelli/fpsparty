#ifndef FPSPARTY_GRAPHICS_GRAPHICS_HPP
#define FPSPARTY_GRAPHICS_GRAPHICS_HPP

#include "glfw.hpp"
#include "graphics/index_buffer.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/staging_buffer.hpp"
#include "graphics/vertex_buffer.hpp"
#include "graphics/work.hpp"
#include "graphics/work_recorder.hpp"
#include "graphics/work_resource_pool.hpp"
#include "rc.hpp"
#include <cstddef>
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
struct Graphics_create_info {
  glfw::Window window{};
  vk::SurfaceKHR surface{};
  unsigned max_frames_in_flight{2};
};

class Graphics {
public:
  constexpr Graphics() noexcept = default;

  explicit Graphics(const Graphics_create_info info);

  void poll_works();

  rc::Strong<Pipeline_layout>
  create_pipeline_layout(const Pipeline_layout_create_info &info);

  rc::Strong<Pipeline> create_pipeline(const Pipeline_create_info &info);

  rc::Strong<Staging_buffer>
  create_staging_buffer(std::span<const std::byte> data);

  rc::Strong<Vertex_buffer> create_vertex_buffer(std::size_t size);

  rc::Strong<Index_buffer> create_index_buffer(std::size_t size);

  Work_recorder begin_transient_work();

  rc::Strong<Work> end_transient_work(Work_recorder commands);

  Work_recorder *begin_frame_work();

  rc::Strong<Work> end_frame_work();

private:
  struct Transient_work_resource {
    vk::UniqueCommandPool command_pool;
    vk::CommandBuffer command_buffer;
  };

  struct Frame_resource {
    vk::UniqueSemaphore swapchain_image_acquire_semaphore{};
    vk::UniqueSemaphore swapchain_image_release_semaphore{};
    vk::UniqueFence work_done_fence{};
    vk::UniqueCommandPool command_pool{};
    vk::UniqueCommandBuffer command_buffer{};
    std::uint32_t swapchain_image_index{};
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
  rc::Factory<Pipeline_layout> _pipeline_layout_factory{};
  rc::Factory<Pipeline> _pipeline_factory{};
  rc::Factory<Staging_buffer> _staging_buffer_factory{};
  rc::Factory<Vertex_buffer> _vertex_buffer_factory{};
  rc::Factory<Index_buffer> _index_buffer_factory{};
  detail::Work_resource_pool _work_resource_pool{};
  std::vector<rc::Strong<Work>> _pending_works{};
  std::vector<Frame_resource> _frame_resources{};
  unsigned _frame_resource_index{};
};
} // namespace fpsparty::graphics

#endif
