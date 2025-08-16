#ifndef FPSPARTY_GRAPHICS_WORK_RESOURCE_POOL_HPP
#define FPSPARTY_GRAPHICS_WORK_RESOURCE_POOL_HPP

#include "graphics/work_resource.hpp"

namespace fpsparty::graphics::detail {
class Work_resource_pool {
public:
  Work_resource pop();

  void push(Work_resource resource);

private:
  std::vector<Work_resource> _resources;
  std::mutex _mutex;
};
} // namespace fpsparty::graphics::detail

#endif
