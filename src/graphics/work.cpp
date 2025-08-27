#include "work.hpp"

#include "graphics/global_vulkan_state.hpp"

namespace fpsparty::graphics {
namespace detail {
bool poll_work(Work &work) {
  if (work._done.load()) {
    return true;
  } else if (Global_vulkan_state::get().device().getFenceStatus(
                 *work._resource.vk_fence) == vk::Result::eSuccess) {
    for (const auto &done_callback : work._resource.done_callbacks) {
      done_callback->on_work_done(work.strong_from_this());
    }
    work._resource.done_callbacks.clear();
    work._done.store(true);
    return true;
  } else {
    return false;
  }
}

Work_resource release_work(Work &work) noexcept {
  return std::move(work._resource);
}
} // namespace detail

void Work::add_done_callback(Work_done_callback *done_callback) {
  _resource.done_callbacks.emplace_back(done_callback);
}

bool Work::is_done() const { return _done.load(); }

void Work::await() const {
  if (_done.load()) {
    return;
  }
  std::ignore = Global_vulkan_state::get().device().waitForFences(
      {*_resource.vk_fence}, vk::False,
      std::numeric_limits<std::uint64_t>::max());
}

Work::Work(detail::Work_resource resource) : _resource{std::move(resource)} {}
} // namespace fpsparty::graphics
