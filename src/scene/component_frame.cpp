#include "component_frame.hpp"

#include <algorithm>
#include <cmath>

#include <game/humanoid.hpp>
#include <game/item.hpp>

namespace fpsparty::scene {

Element_frame Component_frame::render() const {
  static_assert(sizeof(net::Entity_id) == sizeof(u32));
  auto result = Element_frame{};
  result.cameras.reserve(cameras.size());
  result.boxes.reserve(boxes.size() + humanoids.size() * 2 + items.size());
  result.distant_lights.reserve(distant_lights.size());
  result.point_lights.reserve(point_lights.size());
  std::ranges::copy(cameras, std::back_inserter(result.cameras));
  std::ranges::copy(boxes, std::back_inserter(result.boxes));
  std::ranges::copy(distant_lights, std::back_inserter(result.distant_lights));
  std::ranges::copy(point_lights, std::back_inserter(result.point_lights));
  for (auto const &humanoid : humanoids) {
    auto constexpr body_width = game::Humanoid::half_width * 2.0f;
    auto constexpr body_depth = 0.25f;
    auto constexpr head_size = 0.5f;
    auto constexpr body_height = game::Humanoid::height - head_size;
    auto const yaw = math::quat{Eigen::AngleAxisf{
      humanoid.yaw, math::vec3::UnitY()}};
    auto const pitch = math::quat{Eigen::AngleAxisf{
      humanoid.pitch, math::vec3::UnitX()}};
    auto const head_orientation = yaw * pitch;
    auto const head_pivot =
      (humanoid.position + math::vec3::UnitY() * body_height).eval();
    auto const head_offset =
      (pitch * (math::vec3::UnitY() * (head_size * 0.5f))).eval();
    result.boxes.push_back({
      .key = elements::entity_part_key(humanoid.entity_id, 0),
      .half_extents = {body_width * 0.5f, body_height * 0.5f,
                       body_depth * 0.5f},
      .position = humanoid.position +
        math::vec3::UnitY() * (body_height * 0.5f),
      .orientation = yaw,
    });
    result.boxes.push_back({
      .key = elements::entity_part_key(humanoid.entity_id, 1),
      .half_extents = math::vec3::Constant(head_size * 0.5f),
      .position = (head_pivot + yaw * head_offset).eval(),
      .orientation = head_orientation,
    });
  }
  for (auto const &item : items) {
    result.boxes.push_back({
      .key = elements::entity_part_key(item.entity_id, 0),
      .half_extents = math::vec3::Constant(game::Item::half_extent),
      .position = item.position,
      .orientation = math::quat::Identity(),
    });
  }
  return result;
}

Component_frame_index::Component_frame_index(Component_frame const &frame) { 
  camera_indices.reserve(frame.cameras.size());
  distant_light_indices.reserve(frame.distant_lights.size());
  point_light_indices.reserve(frame.point_lights.size());
  box_indices.reserve(frame.boxes.size());
  humanoid_indices.reserve(frame.humanoids.size());
  item_indices.reserve(frame.items.size());
  assert(frame.cameras.size() <= std::numeric_limits<u32>::max());
  assert(frame.distant_lights.size() <= std::numeric_limits<u32>::max());
  assert(frame.point_lights.size() <= std::numeric_limits<u32>::max());
  assert(frame.boxes.size() <= std::numeric_limits<u32>::max());
  assert(frame.humanoids.size() <= std::numeric_limits<u32>::max());
  assert(frame.items.size() <= std::numeric_limits<u32>::max());
  for (auto i = u32{}; i != frame.cameras.size(); ++i) {
    auto const b = camera_indices.try_emplace(frame.cameras[i].key, i);
    assert(b.second);
  }
  for (auto i = u32{}; i != frame.distant_lights.size(); ++i) {
    auto const b = distant_light_indices.try_emplace(frame.distant_lights[i].key, i);
    assert(b.second);
  }
  for (auto i = u32{}; i != frame.point_lights.size(); ++i) {
    auto const b = point_light_indices.try_emplace(frame.point_lights[i].key, i);
    assert(b.second);
  }
  for (auto i = u32{}; i != frame.boxes.size(); ++i) {
    auto const b = box_indices.try_emplace(frame.boxes[i].key, i);
    assert(b.second);
  }
  for (auto i = u32{}; i != frame.humanoids.size(); ++i) {
    auto const b = humanoid_indices.try_emplace(frame.humanoids[i].entity_id, i);
    assert(b.second);
  }
  for (auto i = u32{}; i != frame.items.size(); ++i) {
    auto const b = item_indices.try_emplace(frame.items[i].entity_id, i);
    assert(b.second);
  }
}

Component_frame
interpolate(
  Component_frame const &a,
  Component_frame const &b,
  Component_frame_index const &b_index,
  float t) {
  auto result = a;

  for (auto &camera : result.cameras) {
    auto const i = b_index.camera_indices.find(camera.key);
    if (i == b_index.camera_indices.end()) {
      continue;
    }
    auto const &next = b.cameras[i->value()];
    camera.position = ((1.0f - t) * camera.position + t * next.position).eval();
    camera.pitch = std::lerp(camera.pitch, next.pitch, t);
    camera.yaw = std::lerp(camera.yaw, next.yaw, t);
  }
  for (auto &light : result.distant_lights) {
    auto const i = b_index.distant_light_indices.find(light.key);
    if (i == b_index.distant_light_indices.end()) {
      continue;
    }
    auto const &next = b.distant_lights[i->value()];
    light.direction =
      ((1.0f - t) * light.direction + t * next.direction).normalized().eval();
    light.irradiance =
      ((1.0f - t) * light.irradiance + t * next.irradiance).eval();
  }
  for (auto &light : result.point_lights) {
    auto const i = b_index.point_light_indices.find(light.key);
    if (i == b_index.point_light_indices.end()) {
      continue;
    }
    auto const &next = b.point_lights[i->value()];
    light.position = ((1.0f - t) * light.position + t * next.position).eval();
    light.irradiance =
      ((1.0f - t) * light.irradiance + t * next.irradiance).eval();
  }
  for (auto &box : result.boxes) {
    auto const i = b_index.box_indices.find(box.key);
    if (i == b_index.box_indices.end()) {
      continue;
    }
    auto const &next = b.boxes[i->value()];
    box.half_extents =
      ((1.0f - t) * box.half_extents + t * next.half_extents).eval();
    box.position = ((1.0f - t) * box.position + t * next.position).eval();
    box.orientation = box.orientation.slerp(t, next.orientation);
  }
  for (auto &humanoid : result.humanoids) {
    auto const i = b_index.humanoid_indices.find(humanoid.entity_id);
    if (i == b_index.humanoid_indices.end()) {
      continue;
    }
    auto const &next = b.humanoids[i->value()];
    humanoid.position =
      ((1.0f - t) * humanoid.position + t * next.position).eval();
    humanoid.pitch = std::lerp(humanoid.pitch, next.pitch, t);
    humanoid.yaw = std::lerp(humanoid.yaw, next.yaw, t);
  }
  for (auto &item : result.items) {
    auto const i = b_index.item_indices.find(item.entity_id);
    if (i == b_index.item_indices.end()) {
      continue;
    }
    auto const &next = b.items[i->value()];
    item.position = ((1.0f - t) * item.position + t * next.position).eval();
  }

  return result;
}

}
