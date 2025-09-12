#include "work.hpp"

#include "algorithms/unordered_erase.hpp"
#include "graphics/global_vulkan_state.hpp"

namespace fpsparty::graphics {
namespace detail {
bool poll_work(Work &work) {
  if (work._done.load()) {
    return true;
  } else if (Global_vulkan_state::get().device().getFenceStatus(
                 *work._resource.vk_fence) == vk::Result::eSuccess) {
    work._done.store(true);
    for (const auto &done_callback : work._resource.done_callbacks) {
      done_callback->on_work_done(work);
    }
    work._resource.done_callbacks.clear();
    return true;
  } else {
    return false;
  }
}

Work_resource release_work(Work &work) noexcept {
  return std::move(work._resource);
}
} // namespace detail

void Work::add_done_callback(Work_done_callback *callback) {
  if (is_done()) {
    callback->on_work_done(*this);
  } else {
    _resource.done_callbacks.emplace_back(callback);
  }
}

void Work::remove_done_callback(Work_done_callback *callback) {
  algorithms::unordered_erase_one_if(
      _resource.done_callbacks, [&](Work_done_callback *contained_callback) {
        return contained_callback == callback;
      });
}

bool Work::is_done() const { return _done.load(); }

void Work::await() const {
  if (!is_done()) {
    std::ignore = Global_vulkan_state::get().device().waitForFences(
        {*_resource.vk_fence},
        vk::False,
        std::numeric_limits<std::uint64_t>::max());
  }
}

Work::Work(detail::Work_resource resource) : _resource{std::move(resource)} {}
} // namespace fpsparty::graphics
