#ifndef FPSPARTY_GRAPHICS_WORK_HPP
#define FPSPARTY_GRAPHICS_WORK_HPP

#include "graphics/work_done_callback.hpp"
#include "graphics/work_resource.hpp"
#include "int.hpp"
#include "rc.hpp"

namespace fpsparty::graphics {
class Work;

namespace detail {
bool poll_work(Work &work);

Work_resource release_work(Work &work) noexcept;
} // namespace detail

class Work : public rc::From_this<Work> {
public:
  void add_done_callback(Work_done_callback *callback);

  void remove_done_callback(Work_done_callback *callback);

  bool is_done() const;

  void await() const;

  // Value this submission signaled on Work_queue's shared timeline
  // semaphore. Pass the owning Work to Graphics::submit_frame_work/
  // submit_transient_work's wait_for parameter to make a later
  // submission's GPU execution wait on this one having completed,
  // without a CPU-side stall.
  u64 timeline_value() const noexcept { return _resource.timeline_value; }

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
