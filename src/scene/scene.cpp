#include "scene.hpp"

#include <cmath>

#include <Eigen/Geometry>

namespace fpsparty::scene {

namespace {}

Scene::Scene(Scene_create_info const &info)
    : _max_buffered_keyframes{info.max_buffered_keyframes},
      _keyframe_duration{info.keyframe_duration} {
  assert(info.max_buffered_keyframes > 0);
}

void Scene::push_keyframe(Keyframe &&keyframe) {
  assert(
    _indexed_keyframes.empty() ||
    keyframe.number > _indexed_keyframes.back().keyframe.number);
  if (
    _indexed_keyframes.empty() ||
    (_indexed_keyframes.size() == _max_buffered_keyframes)) {
    auto const &next_base_keyframe =
      _indexed_keyframes.empty() || _max_buffered_keyframes == 1
        ? keyframe
        : _indexed_keyframes[1].keyframe;
    _keyframe_number = next_base_keyframe.number;
    _inter_keyframe_time = 0.0f;
    _grid_remesh_flag =
      _indexed_keyframes.empty()
        ? true
        : game::Grid::diff(get_grid(), next_base_keyframe.grid);
  } else if (keyframe.number < _keyframe_number) {
    return;
  }
  auto const camera_count = keyframe.cameras.size();
  auto const mesh_instance_count = keyframe.mesh_instances.size();
  auto indexed_keyframe = Indexed_keyframe{
    .keyframe = std::move(keyframe),
    .camera_indices =
      Hash_table{
        object_count_to_bucket_count(camera_count),
      },
    .mesh_instance_indices =
      Hash_table{
        object_count_to_bucket_count(mesh_instance_count),
      },
  };
  for (auto const &camera : indexed_keyframe.keyframe.cameras) {
    indexed_keyframe.camera_indices
      .insert(camera.id, &camera - &indexed_keyframe.keyframe.cameras[0]);
  }
  for (auto const &mesh_instance : indexed_keyframe.keyframe.mesh_instances) {
    indexed_keyframe.mesh_instance_indices.insert(
      mesh_instance.id,
      &mesh_instance - &indexed_keyframe.keyframe.mesh_instances[0]);
  }
  if (_indexed_keyframes.size() == _max_buffered_keyframes) {
    _indexed_keyframes.erase(_indexed_keyframes.begin());
    _interpolation.clear();
  }
  _indexed_keyframes.emplace_back(std::move(indexed_keyframe));
}

bool Scene::update(float duration) {
  assert(duration > 0.0f);
  assert(_indexed_keyframes.size() > 0);
  // no matter what, we're going to keep time.
  // it is the consumer's responsibility to wait for a buffer of keyframes
  _inter_keyframe_time += duration / _keyframe_duration;
  if (_inter_keyframe_time >= 1.0f) {
    auto const increment = static_cast<std::uint64_t>(_inter_keyframe_time);
    _keyframe_number += increment;
    _inter_keyframe_time -= increment;
  }
  auto const old_keyframe_count = count_old_keyframes();
  auto const erase_count = old_keyframe_count > 0 ? old_keyframe_count - 1 : 0;
  bool redraw = false;
  if (erase_count > 0) {
    redraw = true;
    auto const &curr_grid = get_grid();
    auto const &next_grid = _indexed_keyframes[erase_count].keyframe.grid;
    if (game::Grid::diff(curr_grid, next_grid)) {
      _grid_remesh_flag = true;
    }
    auto const erase_end = _indexed_keyframes.begin() + erase_count;
    _indexed_keyframes.erase(_indexed_keyframes.begin(), erase_end);
  }
  _interpolation.clear();
  if (_indexed_keyframes.size() >= 2) {
    redraw = true;
    interpolate();
  }
  return redraw;
}

std::size_t Scene::count_old_keyframes() const noexcept {
  auto old_keyframe_count = std::size_t{};
  while (old_keyframe_count < _indexed_keyframes.size()) {
    if (
      _indexed_keyframes[old_keyframe_count].keyframe.number <=
      _keyframe_number) {
      ++old_keyframe_count;
    } else {
      break;
    }
  }
  return old_keyframe_count;
}

void Scene::interpolate() {
  // Note: keeps objects even if they're not in the next keyframe
  // Reason: we can reuse front keyframes index map for interpolated objects.
  auto const a = _indexed_keyframes[0].keyframe.number;
  auto const b = _indexed_keyframes[1].keyframe.number;
  auto const t = (_keyframe_number - a + _inter_keyframe_time) / (b - a);
  for (auto const &[id, curr_camera] : _indexed_keyframes[0].keyframe.cameras) {
    if (auto const idx = _indexed_keyframes[1].camera_indices.get(id)) {
      auto const &next_camera =
        _indexed_keyframes[1].keyframe.cameras[*idx].value;
      _interpolation.cameras.emplace_back(
        id,
        Camera{
          .position =
            (1.0f - t) * curr_camera.position + t * next_camera.position,
          .yaw = std::lerp(curr_camera.yaw, next_camera.yaw, t),
          .pitch = std::lerp(curr_camera.pitch, next_camera.pitch, t),
        });
    } else {
      _interpolation.cameras.emplace_back(id, curr_camera);
    }
  }
  for (auto const &[id, curr_mesh_instance] :
       _indexed_keyframes[0].keyframe.mesh_instances) {
    if (auto const idx = _indexed_keyframes[1].mesh_instance_indices.get(id)) {
      auto const &next_mesh_instance =
        _indexed_keyframes[1].keyframe.mesh_instances[*idx].value;
      _interpolation.mesh_instances.emplace_back(
        id,
        Mesh_instance{
          .mesh = curr_mesh_instance.mesh,
          .position = (1.0f - t) * curr_mesh_instance.position +
                      t * next_mesh_instance.position,
          .orientation = curr_mesh_instance.orientation
                           .slerp(t, next_mesh_instance.orientation),
          .scale = (1.0f - t) * curr_mesh_instance.scale +
                   t * next_mesh_instance.scale,
        });
    } else {
      _interpolation.mesh_instances.emplace_back(id, curr_mesh_instance);
    }
  }
  _interpolation.valid = true;
}

game::Grid const &Scene::get_grid() const noexcept {
  return _indexed_keyframes.front().keyframe.grid;
}

bool Scene::get_grid_remesh_flag() const noexcept { return _grid_remesh_flag; }

void Scene::reset_grid_remesh_flag() noexcept { _grid_remesh_flag = false; }

std::span<Identified<Camera> const>
Scene::get_interpolated_cameras() const noexcept {
  assert(!_indexed_keyframes.empty());
  if (_interpolation.valid) {
    return _interpolation.cameras;
  } else {
    return _indexed_keyframes.front().keyframe.cameras;
  }
}

Camera const *Scene::get_interpolated_camera(std::uint64_t id) const noexcept {
  assert(!_indexed_keyframes.empty());
  auto const index = _indexed_keyframes.front().camera_indices.get(id);
  if (index) {
    if (_interpolation.valid) {
      return &_interpolation.cameras[*index].value;
    } else {
      return &_indexed_keyframes.front().keyframe.cameras[*index].value;
    }
  } else {
    return nullptr;
  }
}

std::span<Identified<Mesh_instance> const>
Scene::get_interpolated_mesh_instances() const noexcept {
  assert(!_indexed_keyframes.empty());
  if (_interpolation.valid) {
    return _interpolation.mesh_instances;
  } else {
    return _indexed_keyframes.front().keyframe.mesh_instances;
  }
}

std::size_t Scene::get_keyframe_count() const noexcept {
  return _indexed_keyframes.size();
}

std::size_t Scene::get_max_buffered_keyframes() const noexcept {
  return _max_buffered_keyframes;
}

float Scene::get_keyframe_duration() const noexcept {
  return _keyframe_duration;
}

} // namespace fpsparty::scene
