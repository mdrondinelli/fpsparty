#include "work_queue.hpp"
#include "algorithms/unordered_erase.hpp"
#include "graphics/global_vulkan_state.hpp"
#include "graphics/work.hpp"
#include <mutex>

namespace fpsparty::graphics::detail {
void Work_queue::poll(Work_resource_pool &resource_pool) {
  const auto pending_works_lock = std::scoped_lock{_pending_works_mutex};
  for (const auto &work : _pending_works) {
    if (detail::poll_work(*work)) {
      auto resource = detail::release_work(*work);
      detail::reset_work_resource(resource);
      resource_pool.push(std::move(resource));
    }
  }
  algorithms::unordered_erase_many_if(
      _pending_works,
      [&](const rc::Strong<Work> &work) { return work->is_done(); });
}

rc::Strong<Work> Work_queue::submit(const Work_queue_submit_info &info) {
  const auto wait_stage =
      vk::PipelineStageFlags{vk::PipelineStageFlagBits::eTopOfPipe};
  auto vk_submit_info = vk::SubmitInfo{
      .commandBufferCount = 1,
      .pCommandBuffers = &info.resource->vk_command_buffer,
  };
  if (info.wait_semaphore) {
    vk_submit_info.waitSemaphoreCount = 1;
    vk_submit_info.pWaitSemaphores = &info.wait_semaphore;
    vk_submit_info.pWaitDstStageMask = &wait_stage;
  }
  if (info.signal_semaphore) {
    vk_submit_info.signalSemaphoreCount = 1;
    vk_submit_info.pSignalSemaphores = &info.signal_semaphore;
  }
  Global_vulkan_state::get().submit({vk_submit_info}, *info.resource->vk_fence);
  auto work = _work_factory.create(std::move(*info.resource));
  const auto pending_works_lock = std::scoped_lock{_pending_works_mutex};
  _pending_works.emplace_back(work);
  return work;
}
} // namespace fpsparty::graphics::detail
