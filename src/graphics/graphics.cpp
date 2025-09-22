#include "graphics.hpp"
#include "glfw.hpp"
#include "graphics/buffer_usage.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/image.hpp"
#include "graphics/work_recorder.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics {
namespace {
std::tuple<vk::Format, vk::Extent2D, vk::UniqueSwapchainKHR> make_swapchain(
  glfw::Window window,
  vk::SurfaceKHR surface,
  vk::PresentModeKHR present_mode) {
  const auto capabilities = Global_vulkan_state::get()
                              .physical_device()
                              .getSurfaceCapabilitiesKHR(surface);
  const auto image_count =
    capabilities.maxImageCount > 0
      ? std::min(capabilities.maxImageCount, capabilities.minImageCount + 1)
      : (capabilities.minImageCount + 1);
  const auto surface_format = [&]() {
    const auto surface_formats = Global_vulkan_state::get()
                                   .physical_device()
                                   .getSurfaceFormatsKHR(surface);
    for (const auto &surface_format : surface_formats) {
      if (
        surface_format.format == vk::Format::eB8G8R8A8Srgb &&
        surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return surface_format;
      }
    }
    throw std::runtime_error{"No acceptable surface format available"};
  }();
  auto const extent = [&]() {
    if (
      capabilities.currentExtent.width !=
      std::numeric_limits<std::uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      const auto framebuffer_size = window.get_framebuffer_size();
      return vk::Extent2D{
        .width = std::clamp(
          static_cast<std::uint32_t>(framebuffer_size[0]),
          capabilities.minImageExtent.width,
          capabilities.maxImageExtent.width),
        .height = std::clamp(
          static_cast<std::uint32_t>(framebuffer_size[1]),
          capabilities.minImageExtent.height,
          capabilities.maxImageExtent.height),
      };
    }
  }();
  return std::tuple{
    surface_format.format,
    extent,
    Global_vulkan_state::get().device().createSwapchainKHRUnique({
      .surface = surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = present_mode,
      .clipped = true,
    }),
  };
}

std::vector<vk::UniqueImageView> make_swapchain_image_views(
  std::span<const vk::Image> swapchain_images,
  vk::Format swapchain_image_format) {
  auto retval = std::vector<vk::UniqueImageView>{};
  retval.reserve(swapchain_images.size());
  for (const auto image : swapchain_images) {
    retval.emplace_back(
      Global_vulkan_state::get().device().createImageViewUnique({
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = swapchain_image_format,
        .subresourceRange =
          {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
          },
      }));
  }
  return retval;
}

vk::UniqueSemaphore make_semaphore(
  const char *
#ifndef FPSPARTY_VULKAN_NDEBUG
    debug_name
#endif
) {
  auto retval = Global_vulkan_state::get().device().createSemaphoreUnique({});
#ifndef FPSPARTY_VULKAN_NDEBUG
  Global_vulkan_state::get().device().setDebugUtilsObjectNameEXT({
    .objectType = vk::ObjectType::eSemaphore,
    .objectHandle = std::bit_cast<std::uint64_t>(*retval),
    .pObjectName = debug_name,
  });
#endif
  return retval;
}
} // namespace

Graphics::Graphics(const Graphics_create_info &info)
    : _window{info.window},
      _surface{info.surface},
      _surface_present_modes{Global_vulkan_state::get()
                               .physical_device()
                               .getSurfacePresentModesKHR(_surface)},
      _vsync_preferred{info.vsync_preferred} {
  init_swapchain(select_swapchain_present_mode());
  for (auto i = std::size_t{}; i != info.max_frames_in_flight; ++i) {
    const auto swapchain_image_acquire_semaphore_name =
      "swapchain_image_acquire_semaphores[" + std::to_string(i) + "]";
    const auto swapchain_image_release_semaphore_name =
      "swapchain_image_release_semaphores[" + std::to_string(i) + "]";
    _frame_resources.push_back({
      .swapchain_image_acquire_semaphore =
        make_semaphore(swapchain_image_acquire_semaphore_name.c_str()),
      .swapchain_image_release_semaphore =
        make_semaphore(swapchain_image_release_semaphore_name.c_str()),
    });
  }
}

void Graphics::poll_works() {
  ZoneScoped;
  _works.poll(_work_resources);
}

rc::Strong<Pipeline_layout>
Graphics::create_pipeline_layout(const Pipeline_layout_create_info &info) {
  return _pipeline_layout_factory.create(info);
}

rc::Strong<Pipeline>
Graphics::create_graphics_pipeline(const Graphics_pipeline_create_info &info) {
  return _pipeline_factory.create(info);
}

rc::Strong<Buffer> Graphics::create_buffer(const Buffer_create_info &info) {
  return _buffer_factory.create(info);
}

rc::Strong<Buffer> Graphics::create_staging_buffer(std::size_t size) {
  return _buffer_factory.create(
    Buffer_create_info{
      .size = size,
      .usage = Buffer_usage_flag_bits::transfer_src,
      .mapping_mode = Mapping_mode::write_only,
    });
}

rc::Strong<Buffer>
Graphics::create_staging_buffer(std::span<const std::byte> data) {
  auto retval = create_staging_buffer(data.size());
  const auto memory = retval->map();
  std::memcpy(memory.get().data(), data.data(), data.size());
  return retval;
}

rc::Strong<Buffer> Graphics::create_vertex_buffer(std::size_t size) {
  return _buffer_factory.create(
    Buffer_create_info{
      .size = size,
      .usage = Buffer_usage_flag_bits::transfer_dst |
               Buffer_usage_flag_bits::vertex_buffer,
    });
}

rc::Strong<Buffer> Graphics::create_index_buffer(std::size_t size) {
  return _buffer_factory.create(
    Buffer_create_info{
      .size = size,
      .usage = Buffer_usage_flag_bits::transfer_dst |
               Buffer_usage_flag_bits::index_buffer,
    });
}

rc::Strong<Image> Graphics::create_image(const Image_create_info &info) {
  return _image_factory.create(info);
}

Work_recorder Graphics::record_transient_work() {
  return detail::acquire_work_recorder(_work_resources.pop());
}

rc::Strong<Work> Graphics::submit_transient_work(Work_recorder recorder) {
  auto resource = detail::release_work_recorder(std::move(recorder));
  return _works.submit({.resource = &resource});
}

std::pair<Work_recorder, rc::Strong<Image>> Graphics::record_frame_work() {
  ZoneScoped;
  auto &frame_resource = _frame_resources[_frame_resource_index];
  frame_resource.swapchain_image_index = [&]() {
    for (;;) {
      try {
        const auto swapchain_image_index =
          Global_vulkan_state::get().device().acquireNextImageKHR(
            *_swapchain,
            std::numeric_limits<std::uint64_t>::max(),
            *frame_resource.swapchain_image_acquire_semaphore);
        return swapchain_image_index.value;
      } catch (const vk::OutOfDateKHRError &e) {
        deinit_swapchain();
        init_swapchain(select_swapchain_present_mode());
      }
    }
  }();
  if (frame_resource.pending_work) {
    frame_resource.pending_work->await();
    frame_resource.pending_work = nullptr;
  }
  return {
    detail::acquire_work_recorder(_work_resources.pop()),
    _swapchain_images.at(frame_resource.swapchain_image_index),
  };
}

rc::Strong<Work> Graphics::submit_frame_work(Work_recorder recorder) {
  ZoneScoped;
  auto &frame_resource = _frame_resources[_frame_resource_index];
  auto work_resource = detail::release_work_recorder(std::move(recorder));
  frame_resource.pending_work = _works.submit({
    .resource = &work_resource,
    .wait_semaphore = *frame_resource.swapchain_image_acquire_semaphore,
    .signal_semaphore = *frame_resource.swapchain_image_release_semaphore,
  });
  try {
    const auto present_result = Global_vulkan_state::get().present({
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*frame_resource.swapchain_image_release_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &*_swapchain,
      .pImageIndices = &frame_resource.swapchain_image_index,
    });
    if (present_result == vk::Result::eSuboptimalKHR) {
      throw vk::OutOfDateKHRError{"Subobtimal queue present result"};
    }
    const auto framebuffer_size = _window.get_framebuffer_size();
    if (
      _swapchain_image_extent.width !=
        static_cast<std::uint32_t>(framebuffer_size[0]) ||
      _swapchain_image_extent.height !=
        static_cast<std::uint32_t>(framebuffer_size[1])) {
      throw vk::OutOfDateKHRError{"Framebuffer size mismatch"};
    }
    if (const auto selected_swapchain_present_mode =
          select_swapchain_present_mode();
        _swapchain_present_mode != selected_swapchain_present_mode) {
      throw vk::OutOfDateKHRError{"Present mode mismatch"};
    }
  } catch (const vk::OutOfDateKHRError &e) {
    deinit_swapchain();
    init_swapchain(select_swapchain_present_mode());
  }
  _frame_resource_index = (_frame_resource_index + 1) % _frame_resources.size();
  return frame_resource.pending_work;
}

bool Graphics::is_vsync_preferred() const noexcept { return _vsync_preferred; }

void Graphics::set_vsync_preferred(bool value) { _vsync_preferred = value; }

void Graphics::init_swapchain(vk::PresentModeKHR present_mode) {
  assert(!_swapchain);
  assert(_vk_swapchain_images.empty());
  assert(_vk_swapchain_image_views.empty());
  assert(_swapchain_images.empty());
  std::tie(_swapchain_image_format, _swapchain_image_extent, _swapchain) =
    make_swapchain(_window, _surface, present_mode);
  _swapchain_present_mode = present_mode;
  _vk_swapchain_images =
    Global_vulkan_state::get().device().getSwapchainImagesKHR(*_swapchain);
  _vk_swapchain_image_views =
    make_swapchain_image_views(_vk_swapchain_images, _swapchain_image_format);
  for (auto i = std::size_t{}; i != _vk_swapchain_images.size(); ++i) {
    _swapchain_images.emplace_back(_image_factory.create(
      detail::External_image_create_info{
        .image = _vk_swapchain_images[i],
        .image_view = *_vk_swapchain_image_views[i],
        .format = static_cast<Image_format>(_swapchain_image_format),
        .extent =
          {
            static_cast<int>(_swapchain_image_extent.width),
            static_cast<int>(_swapchain_image_extent.height),
            1,
          },
        .mip_level_count = 1,
        .array_layer_count = 1,
      }));
  }
}

void Graphics::deinit_swapchain() {
  Global_vulkan_state::get().device().waitIdle();
  _swapchain_images.clear();
  _vk_swapchain_image_views.clear();
  _vk_swapchain_images.clear();
  _swapchain.reset();
}

vk::PresentModeKHR Graphics::select_swapchain_present_mode() const noexcept {
  const auto preferred_present_mode = _vsync_preferred
                                        ? vk::PresentModeKHR::eMailbox
                                        : vk::PresentModeKHR::eImmediate;
  for (const auto present_mode : _surface_present_modes) {
    if (present_mode == preferred_present_mode) {
      return present_mode;
    }
  }
  return vk::PresentModeKHR::eFifo;
}
} // namespace fpsparty::graphics
