#include "work_resource_pool.hpp"
#include <mutex>

namespace fpsparty::graphics::detail {
Work_resource Work_resource_pool::pop() {
  auto lock = std::unique_lock{_mutex};
  if (!_resources.empty()) {
    auto resource = std::move(_resources.back());
    _resources.pop_back();
    return resource;
  } else {
    lock.unlock();
    return detail::create_work_resource();
  }
}

void Work_resource_pool::push(Work_resource resource) {
  const auto lock = std::scoped_lock{_mutex};
  _resources.emplace_back(std::move(resource));
}
} // namespace fpsparty::graphics::detail
