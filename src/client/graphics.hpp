#ifndef FPSPARTY_CLIENT_GRAPHICS_HPP
#define FPSPARTY_CLIENT_GRAPHICS_HPP

#include "glfw.hpp"
#include <Eigen/Dense>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace fpsparty::client {
class Graphics {
public:
  struct Create_info {
    glfw::Window window{};
    vk::SurfaceKHR surface{};
    unsigned max_frames_in_flight{2};
  };

  constexpr Graphics() noexcept = default;

  explicit Graphics(const Create_info &info);

  [[nodiscard]] bool begin();
  void end();

  void bind_vertex_buffer(vk::Buffer buffer) noexcept;
  void bind_index_buffer(vk::Buffer buffer, vk::IndexType index_type) noexcept;
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
    vk::UniqueImageView depth_image_view{};
    std::uint32_t swapchain_image_index{};
  };

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
  std::vector<Frame_resource> _frame_resources{};
  unsigned _frame_resource_index{};
};
} // namespace fpsparty::client

#endif
