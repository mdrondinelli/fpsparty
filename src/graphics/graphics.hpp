#ifndef FPSPARTY_GRAPHICS_GRAPHICS_HPP
#define FPSPARTY_GRAPHICS_GRAPHICS_HPP

#include "glfw.hpp"
#include "graphics/buffer.hpp"
#include "graphics/image.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/work.hpp"
#include "graphics/work_queue.hpp"
#include "graphics/work_recorder.hpp"
#include "graphics/work_resource_pool.hpp"
#include "rc.hpp"
#include <cstddef>
#include <span>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
struct Graphics_create_info {
  glfw::Window window;
  vk::SurfaceKHR surface;
  bool vsync_preferred{true};
  unsigned max_frames_in_flight{2};
};

class Graphics {
public:
  constexpr Graphics() noexcept = default;

  explicit Graphics(const Graphics_create_info &info);

  void poll_works();

  rc::Strong<Pipeline_layout>
  create_pipeline_layout(const Pipeline_layout_create_info &info);

  rc::Strong<Pipeline> create_pipeline(const Pipeline_create_info &info);

  rc::Strong<Buffer> create_staging_buffer(std::span<const std::byte> data);

  rc::Strong<Buffer> create_vertex_buffer(std::size_t size);

  rc::Strong<Buffer> create_index_buffer(std::size_t size);

  rc::Strong<Image> create_image(const Image_create_info &info);

  Work_recorder record_transient_work();

  rc::Strong<Work> submit_transient_work(Work_recorder recorder);

  std::pair<Work_recorder, rc::Strong<Image>> record_frame_work();

  rc::Strong<Work> submit_frame_work(Work_recorder recorder);

  bool is_vsync_preferred() const noexcept;

  void set_vsync_preferred(bool value);

private:
  struct Frame_resource {
    vk::UniqueSemaphore swapchain_image_acquire_semaphore{};
    vk::UniqueSemaphore swapchain_image_release_semaphore{};
    std::uint32_t swapchain_image_index{};
    rc::Strong<Work> pending_work{};
  };

  void init_swapchain(vk::PresentModeKHR present_mode);

  void deinit_swapchain();

  vk::PresentModeKHR select_swapchain_present_mode() const noexcept;

  glfw::Window _window{};
  vk::SurfaceKHR _surface{};
  std::vector<vk::PresentModeKHR> _surface_present_modes{};
  bool _vsync_preferred{};
  rc::Factory<Pipeline_layout> _pipeline_layout_factory{};
  rc::Factory<Pipeline> _pipeline_factory{};
  rc::Factory<Buffer> _buffer_factory{};
  rc::Factory<Image> _image_factory{};
  vk::Format _swapchain_image_format{};
  vk::Extent2D _swapchain_image_extent{};
  vk::PresentModeKHR _swapchain_present_mode{};
  vk::UniqueSwapchainKHR _swapchain{};
  std::vector<vk::Image> _vk_swapchain_images{};
  std::vector<vk::UniqueImageView> _vk_swapchain_image_views{};
  std::vector<rc::Strong<Image>> _swapchain_images{};
  detail::Work_resource_pool _work_resources{};
  detail::Work_queue _works{};
  std::vector<Frame_resource> _frame_resources{};
  unsigned _frame_resource_index{};
};
} // namespace fpsparty::graphics

#endif
