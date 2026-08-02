#ifndef FPSPARTY_GRAPHICS_WORK_QUEUE_HPP
#define FPSPARTY_GRAPHICS_WORK_QUEUE_HPP

#include "graphics/work.hpp"
#include "graphics/work_resource.hpp"
#include "graphics/work_resource_pool.hpp"
#include "int.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics::detail {
struct Work_queue_submit_info {
  Work_resource *resource;
  vk::Semaphore wait_semaphore{};
  vk::Semaphore signal_semaphore{};
  // Value to wait for on the queue's own shared timeline semaphore before
  // this submission's GPU execution begins -- 0 (the default) means no
  // wait. See Work::timeline_value.
  u64 wait_timeline_value{};
};

class Work_queue {
public:
  Work_queue();

  void poll(Work_resource_pool &resource_pool);

  rc::Strong<Work> submit(Work_queue_submit_info const &info);

private:
  rc::Factory<Work> _work_factory{};
  std::vector<rc::Strong<Work>> _pending_works{};
  std::mutex _pending_works_mutex{};
  vk::UniqueSemaphore _timeline_semaphore;
  u64 _next_timeline_value{1};
};
} // namespace fpsparty::graphics::detail

#endif
