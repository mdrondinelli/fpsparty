#include "scene.hpp"

#include <cmath>

namespace fpsparty::scene {

namespace {

template <typename T, typename Map>
T const *find_element(
  std::vector<T> const &elements, Map const &indices, u64 key) noexcept {
  auto const i = indices.find(key);
  return i == indices.end() ? nullptr : &elements[i->value()];
}

} // namespace

Scene::Scene(Scene_create_info const &info)
    : _keyframe_duration{info.keyframe_duration} {
  assert(info.keyframe_duration > 0.0f);
}

void Scene::push(Keyframe &&keyframe) {
  assert(
    empty() || keyframe.number > _indexed_keyframes.back()->keyframe.number);
  if (empty()) {
    _keyframe_number = keyframe.number;
    _inter_keyframe_time = 0.0f;
    _grid_remesh_flag = true;
  }
  _indexed_keyframes.emplace_back(
    std::make_unique<Indexed_keyframe>(std::move(keyframe)));
  if (!_current_frame) {
    _current_frame.emplace(
      _indexed_keyframes.front()->keyframe.components.render());
  }
}

bool Scene::play(float duration) {
  assert(duration > 0.0f);
  assert(!empty());
  _inter_keyframe_time += duration / _keyframe_duration;
  if (_inter_keyframe_time >= 1.0f) {
    auto const increment = static_cast<std::uint64_t>(_inter_keyframe_time);
    _keyframe_number += increment;
    _inter_keyframe_time -= increment;
  }
  if (_keyframe_number >= _indexed_keyframes.back()->keyframe.number) {
    _keyframe_number = _indexed_keyframes.back()->keyframe.number;
    _inter_keyframe_time = 0.0f;
  }
  auto keep_playing = false;
  if (trim_old_keyframes()) {
    keep_playing = true;
  }
  if (interpolate_frames()) {
    keep_playing = true;
  }
  return keep_playing;
}

float Scene::set_latency(float seconds) noexcept {
  assert(seconds >= 0.0f);
  assert(!empty());
  auto const initial_latency = get_latency();
  if (initial_latency < seconds) {
    return initial_latency;
  }
  auto const keyframes = seconds / _keyframe_duration;
  auto const whole_keyframes = static_cast<std::uint64_t>(std::ceil(keyframes));
  _keyframe_number =
    _indexed_keyframes.back()->keyframe.number - whole_keyframes;
  _inter_keyframe_time = whole_keyframes - keyframes;
  trim_old_keyframes();
  interpolate_frames();
  return seconds;
}

float Scene::get_latency() const noexcept {
  assert(!empty());
  return (_indexed_keyframes.back()->keyframe.number - _keyframe_number -
          _inter_keyframe_time) *
    _keyframe_duration;
}

bool Scene::trim_old_keyframes() noexcept {
  auto const old_keyframe_count = count_old_keyframes();
  auto const erase_count = old_keyframe_count > 0 ? old_keyframe_count - 1 : 0;
  auto const &current_grid = get_grid();
  auto const &next_grid = _indexed_keyframes[erase_count]->keyframe.grid;
  if (game::Grid::diff(current_grid, next_grid)) {
    _grid_remesh_flag = true;
  }
  _indexed_keyframes.erase(
    _indexed_keyframes.begin(), _indexed_keyframes.begin() + erase_count);
  return erase_count > 0;
}

std::size_t Scene::count_old_keyframes() const noexcept {
  auto count = std::size_t{};
  while (
    count < _indexed_keyframes.size() &&
    _indexed_keyframes[count]->keyframe.number <= _keyframe_number) {
    ++count;
  }
  return count;
}

void Scene::freeze_previous_frame() {
  assert(_current_frame);
  _previous_frame.emplace(Element_frame{_current_frame->frame});
}

bool Scene::interpolate_frames() {
  if (_indexed_keyframes.size() < 2) {
    freeze_previous_frame();
    return false;
  }
  auto const &a = *_indexed_keyframes[0];
  auto const &b = *_indexed_keyframes[1];
  auto const t =
    (_keyframe_number - a.keyframe.number + _inter_keyframe_time) /
    (b.keyframe.number - a.keyframe.number);
  _previous_frame = std::move(_current_frame);
  // Interpolation retains the elements and ordering of the base keyframe;
  // elements that only occur in the next keyframe appear after it becomes base.
  _current_frame.emplace(
    interpolate(a.keyframe.components, b.keyframe.components, b.index, t)
      .render());
  return true;
}

game::Grid const &Scene::get_grid() const noexcept {
  assert(!empty());
  return _indexed_keyframes.front()->keyframe.grid;
}

bool Scene::get_grid_remesh_flag() const noexcept { return _grid_remesh_flag; }

void Scene::reset_grid_remesh_flag() noexcept { _grid_remesh_flag = false; }

Element_frame const &Scene::get_current_frame() const noexcept {
  assert(_current_frame);
  return _current_frame->frame;
}

Element_frame const *Scene::get_previous_frame() const noexcept {
  return _previous_frame ? &_previous_frame->frame : nullptr;
}

elements::Camera const *Scene::get_camera(u64 key) const noexcept {
  assert(_current_frame);
  return find_element(
    _current_frame->frame.cameras, _current_frame->index.camera_indices, key);
}

elements::Camera const *Scene::get_previous_camera(u64 key) const noexcept {
  return _previous_frame
    ? find_element(
        _previous_frame->frame.cameras,
        _previous_frame->index.camera_indices,
        key)
    : nullptr;
}

elements::Distant_light const *
Scene::get_distant_light(u64 key) const noexcept {
  assert(_current_frame);
  return find_element(
    _current_frame->frame.distant_lights,
    _current_frame->index.distant_light_indices,
    key);
}

elements::Distant_light const *
Scene::get_previous_distant_light(u64 key) const noexcept {
  return _previous_frame
    ? find_element(
        _previous_frame->frame.distant_lights,
        _previous_frame->index.distant_light_indices,
        key)
    : nullptr;
}

elements::Point_light const *Scene::get_point_light(u64 key) const noexcept {
  assert(_current_frame);
  return find_element(
    _current_frame->frame.point_lights,
    _current_frame->index.point_light_indices,
    key);
}

elements::Point_light const *
Scene::get_previous_point_light(u64 key) const noexcept {
  return _previous_frame
    ? find_element(
        _previous_frame->frame.point_lights,
        _previous_frame->index.point_light_indices,
        key)
    : nullptr;
}

elements::Box const *Scene::get_box(u64 key) const noexcept {
  assert(_current_frame);
  return find_element(
    _current_frame->frame.boxes, _current_frame->index.box_indices, key);
}

elements::Box const *Scene::get_previous_box(u64 key) const noexcept {
  return _previous_frame
    ? find_element(
        _previous_frame->frame.boxes, _previous_frame->index.box_indices, key)
    : nullptr;
}

std::size_t Scene::get_keyframe_count() const noexcept {
  return _indexed_keyframes.size();
}

std::uint64_t Scene::get_keyframe_number() const noexcept {
  return _keyframe_number;
}

float Scene::get_inter_keyframe_time() const noexcept {
  return _inter_keyframe_time;
}

float Scene::get_keyframe_duration() const noexcept {
  return _keyframe_duration;
}

bool Scene::empty() const noexcept { return _indexed_keyframes.empty(); }

} // namespace fpsparty::scene
