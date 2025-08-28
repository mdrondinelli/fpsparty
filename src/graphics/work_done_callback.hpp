#ifndef FPSPARTY_GRAPHICS_WORK_DONE_CALLBACK_HPP
#define FPSPARTY_GRAPHICS_WORK_DONE_CALLBACK_HPP

namespace fpsparty::graphics {
class Work;

class Work_done_callback {
public:
  virtual ~Work_done_callback() = default;

  virtual void on_work_done(const Work &work) = 0;
};
} // namespace fpsparty::graphics

#endif
