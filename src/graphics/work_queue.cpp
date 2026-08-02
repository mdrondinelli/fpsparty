#include "work_queue.hpp"
#include "algorithms/unordered_erase.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/work.hpp"
#include <array>
#include <mutex>

namespace fpsparty::graphics::detail {
Work_queue::Work_queue() {
  auto const timeline_create_info = vk::SemaphoreTypeCreateInfo{
    .semaphoreType = vk::SemaphoreType::eTimeline,
    .initialValue = 0,
  };
  _timeline_semaphore =
    Global_vulkan_state::get().device().createSemaphoreUnique(
      {.pNext = &timeline_create_info});
}

void Work_queue::poll(Work_resource_pool &resource_pool) {
  auto const pending_works_lock = std::scoped_lock{_pending_works_mutex};
  for (auto const &work : _pending_works) {
    if (detail::poll_work(*work)) {
      auto resource = detail::release_work(*work);
      detail::reset_work_resource(resource);
      resource_pool.push(std::move(resource));
    }
  }
  algorithms::unordered_erase_many_if(
    _pending_works,
    [&](rc::Strong<Work> const &work) { return work->is_done(); });
}

rc::Strong<Work> Work_queue::submit(Work_queue_submit_info const &info) {
  auto const signal_value = _next_timeline_value++;
  info.resource->timeline_value = signal_value;

  auto wait_infos = std::array<vk::SemaphoreSubmitInfo, 2>{};
  auto wait_count = std::uint32_t{};
  if (info.wait_semaphore) {
    wait_infos[wait_count++] = vk::SemaphoreSubmitInfo{
      .semaphore = info.wait_semaphore,
      .stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
    };
  }
  if (info.wait_timeline_value != 0) {
    wait_infos[wait_count++] = vk::SemaphoreSubmitInfo{
      .semaphore = *_timeline_semaphore,
      .value = info.wait_timeline_value,
      .stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
    };
  }

  auto signal_infos = std::array<vk::SemaphoreSubmitInfo, 2>{
    vk::SemaphoreSubmitInfo{
      .semaphore = *_timeline_semaphore,
      .value = signal_value,
      .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    },
    vk::SemaphoreSubmitInfo{},
  };
  auto signal_count = std::uint32_t{1};
  if (info.signal_semaphore) {
    signal_infos[signal_count++] = vk::SemaphoreSubmitInfo{
      .semaphore = info.signal_semaphore,
      .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
  }

  auto const command_buffer_info = vk::CommandBufferSubmitInfo{
    .commandBuffer = info.resource->vk_command_buffer,
  };
  auto const vk_submit_info = vk::SubmitInfo2{
    .waitSemaphoreInfoCount = wait_count,
    .pWaitSemaphoreInfos = wait_infos.data(),
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &command_buffer_info,
    .signalSemaphoreInfoCount = signal_count,
    .pSignalSemaphoreInfos = signal_infos.data(),
  };
  Global_vulkan_state::get().submit2(
    vk_submit_info, *info.resource->vk_fence);
  auto work = _work_factory.create(std::move(*info.resource));
  auto const pending_works_lock = std::scoped_lock{_pending_works_mutex};
  _pending_works.emplace_back(work);
  return work;
}
} // namespace fpsparty::graphics::detail
