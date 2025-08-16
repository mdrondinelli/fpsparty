#ifndef FPSPARTY_GRAPHICS_WORK_HPP
#define FPSPARTY_GRAPHICS_WORK_HPP

#include "graphics/work_resource.hpp"

namespace fpsparty::graphics {
class Work;

namespace detail {
Work acquire_work(Work_resource resource) noexcept;

Work_resource release_work(Work &work) noexcept;

bool poll_work(Work &work);
} // namespace detail

class Work {
public:
  bool is_done() const;

  void await() const;

private:
  friend Work detail::acquire_work(detail::Work_resource resource) noexcept;

  friend detail::Work_resource detail::release_work(Work &work) noexcept;

  friend bool detail::poll_work(Work &work);

  explicit Work(detail::Work_resource resource);

  mutable std::atomic<bool> _done{false};
  detail::Work_resource _resource;
};
} // namespace fpsparty::graphics

#endif
