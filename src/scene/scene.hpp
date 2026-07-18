#ifndef FPSPARTY_SCENE_SCENE_HPP
#define FPSPARTY_SCENE_SCENE_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "element_frame.hpp"
#include "keyframe.hpp"

namespace fpsparty::scene {

struct Scene_create_info {
  float keyframe_duration;
};

// TODO: clean up API so Scene is constructed with an initial keyframe
class Scene {
public:
  explicit Scene(Scene_create_info const &info);

  /**
   * Appends a keyframe to the timeline.
   *
   * After the first call, the keyframe number must be greater than every
   * previously pushed keyframe number.
   */
  void push(Keyframe &&keyframe);

  /**
   * Advances the timeline by the given positive duration.
   *
   * Returns true when buffered playback can advance, or false when playback is
   * already starved at the most recently pushed keyframe.
   *
   * Requires a non-empty timeline.
   */
  bool play(float duration);

  /**
   * Fast-forwards the timeline to the given non-negative latency.
   *
   * If the requested latency is greater than the current latency, the playback
   * point is unchanged. Returns the effective latency after the call.
   *
   * Requires a non-empty timeline.
   */
  float set_latency(float seconds) noexcept;

  /**
   * Returns the non-negative latency between the playback point and the most
   * recently pushed keyframe.
   *
   * Requires a non-empty timeline.
   */
  float get_latency() const noexcept;

  /**
   * Returns the discrete grid state at the playback point.
   *
   * Requires a non-empty timeline.
   */
  game::Grid const &get_grid() const noexcept;

  /**
   * Returns true when the grid may need remeshing.
   *
   * The flag is set by the first push and whenever advancing playback changes
   * the grid returned by get_grid(). It remains set until reset.
   */
  bool get_grid_remesh_flag() const noexcept;

  /** Clears the grid-remesh flag. */
  void reset_grid_remesh_flag() noexcept;

  /**
   * Returns the current rendered element frame.
   *
   * Before the first playback update this is the first keyframe rendered
   * without interpolation. Requires a non-empty timeline.
   */
  Element_frame const &get_current_frame() const noexcept;

  /**
   * Returns the rendered element frame from the preceding playback update, or
   * null before one has been recorded.
   *
   * A starved playback update freezes the current frame into this history.
   */
  Element_frame const *get_previous_frame() const noexcept;

  /**
   * Returns the current camera with the given key, or null if unknown.
   * Requires a non-empty timeline.
   */
  elements::Camera const *get_camera(u64 key) const noexcept;

  /** Returns the previous camera with the given key, or null if unavailable. */
  elements::Camera const *get_previous_camera(u64 key) const noexcept;

  /**
   * Returns the current distant light with the given key, or null if unknown.
   * Requires a non-empty timeline.
   */
  elements::Distant_light const *get_distant_light(u64 key) const noexcept;

  /**
   * Returns the previous distant light with the given key, or null if
   * unavailable.
   */
  elements::Distant_light const *
  get_previous_distant_light(u64 key) const noexcept;

  /**
   * Returns the current point light with the given key, or null if unknown.
   * Requires a non-empty timeline.
   */
  elements::Point_light const *get_point_light(u64 key) const noexcept;

  /**
   * Returns the previous point light with the given key, or null if
   * unavailable.
   */
  elements::Point_light const *
  get_previous_point_light(u64 key) const noexcept;

  /**
   * Returns the current box with the given key, or null if unknown.
   * Requires a non-empty timeline.
   */
  elements::Box const *get_box(u64 key) const noexcept;

  /** Returns the previous box with the given key, or null if unavailable. */
  elements::Box const *get_previous_box(u64 key) const noexcept;

  /** Returns the number of keyframes currently stored. */
  std::size_t get_keyframe_count() const noexcept;

  /** Returns the integral keyframe number at the playback point. */
  std::uint64_t get_keyframe_number() const noexcept;

  /** Returns the fractional playback position on a scale of [0, 1). */
  float get_inter_keyframe_time() const noexcept;

  /** Returns the keyframe duration supplied at construction. */
  float get_keyframe_duration() const noexcept;

  /** Returns true until the first keyframe is pushed. */
  bool empty() const noexcept;

private:
  struct Indexed_keyframe {
    explicit Indexed_keyframe(Keyframe &&value)
        : keyframe{std::move(value)}, index{keyframe.components} {}

    Keyframe keyframe;
    Component_frame_index index;
  };

  struct Indexed_element_frame {
    explicit Indexed_element_frame(Element_frame &&value)
        : frame{std::move(value)}, index{frame} {}

    Element_frame frame;
    Element_frame_index index;
  };

  bool trim_old_keyframes() noexcept;
  std::size_t count_old_keyframes() const noexcept;
  bool interpolate_frames();
  void freeze_previous_frame();

  float _keyframe_duration;
  std::vector<std::unique_ptr<Indexed_keyframe>> _indexed_keyframes{};
  std::optional<Indexed_element_frame> _previous_frame{};
  std::optional<Indexed_element_frame> _current_frame{};
  std::uint64_t _keyframe_number{};
  float _inter_keyframe_time{};
  bool _grid_remesh_flag{};
};

} // namespace fpsparty::scene

#endif
