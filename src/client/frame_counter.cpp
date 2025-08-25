#include "frame_counter.hpp"
#include <chrono>
#include <numeric>

namespace fpsparty::client {
void Frame_counter::notify() {
  const auto current_notify_time = Clock::now();
  if (_previous_notify_time) {
    _frame_durations.emplace_back(current_notify_time - *_previous_notify_time);
    auto cumulative_frame_duration = std::accumulate(
        _frame_durations.begin(), _frame_durations.end(), Clock::duration{});
    while (cumulative_frame_duration > std::chrono::seconds{1} &&
           _frame_durations.size() > 1) {
      cumulative_frame_duration -= _frame_durations.front();
      _frame_durations.erase(_frame_durations.begin());
    }
  }
  _previous_notify_time = current_notify_time;
}

std::optional<float> Frame_counter::get_frame_duration() const noexcept {
  if (_frame_durations.empty()) {
    return std::nullopt;
  }
  const auto cumulative_frame_duration = std::accumulate(
      _frame_durations.begin(), _frame_durations.end(), Clock::duration{});
  return std::chrono::duration_cast<std::chrono::duration<float>>(
             cumulative_frame_duration)
             .count() /
         _frame_durations.size();
}

std::optional<float> Frame_counter::get_frame_rate() const noexcept {
  if (_frame_durations.empty()) {
    return std::nullopt;
  }
  const auto cumulative_frame_duration = std::accumulate(
      _frame_durations.begin(), _frame_durations.end(), Clock::duration{});
  return _frame_durations.size() /
         std::chrono::duration_cast<std::chrono::duration<float>>(
             cumulative_frame_duration)
             .count();
}
} // namespace fpsparty::client
