#ifndef FPSPARTY_GRAPHICS_WORK_QUEUE_HPP
#define FPSPARTY_GRAPHICS_WORK_QUEUE_HPP

#include "graphics/work.hpp"
#include "graphics/work_resource.hpp"
#include "graphics/work_resource_pool.hpp"
#include <vulkan/vulkan.hpp>

namespace fpsparty::graphics::detail {
struct Work_queue_submit_info {
  Work_resource *resource;
  vk::Semaphore wait_semaphore{};
  vk::Semaphore signal_semaphore{};
};

class Work_queue {
public:
  void poll(Work_resource_pool &resource_pool);

  rc::Strong<Work> submit(const Work_queue_submit_info &info);

private:
  rc::Factory<Work> _work_factory{};
  std::vector<rc::Strong<Work>> _pending_works{};
  std::mutex _pending_works_mutex{};
};
} // namespace fpsparty::graphics::detail

#endif
