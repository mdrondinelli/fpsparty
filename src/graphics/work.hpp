#ifndef FPSPARTY_GRAPHICS_WORK_HPP
#define FPSPARTY_GRAPHICS_WORK_HPP

#include "graphics/work_resource.hpp"

namespace fpsparty::graphics {
class Work;

namespace detail {
bool poll_work(Work &work);

Work_resource release_work(Work &work) noexcept;
} // namespace detail

class Work {
public:
  bool is_done() const;

  void await() const;

private:
  friend class rc::Factory<Work>;

  friend bool detail::poll_work(Work &work);

  friend detail::Work_resource detail::release_work(Work &work) noexcept;

  explicit Work(detail::Work_resource resource);

  mutable std::atomic<bool> _done{false};
  detail::Work_resource _resource;
};
} // namespace fpsparty::graphics

#endif
