#ifndef FPSPARTY_CLIENT_FRAME_COUNTER_HPP
#define FPSPARTY_CLIENT_FRAME_COUNTER_HPP

#include <chrono>
#include <optional>
#include <vector>

namespace fpsparty::client {
class Frame_counter {
public:
  void notify();

  std::optional<float> get_frame_duration() const noexcept;

  std::optional<float> get_frame_rate() const noexcept;

private:
  using Clock = std::chrono::steady_clock;

  std::vector<Clock::duration> _frame_durations{};
  std::optional<Clock::time_point> _previous_notify_time{};
};
} // namespace fpsparty::client

#endif
