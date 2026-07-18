#include "element_frame.hpp"

namespace fpsparty::scene {

Element_frame_index::Element_frame_index(Element_frame const &frame) {
  camera_indices.reserve(frame.cameras.size());
  distant_light_indices.reserve(frame.distant_lights.size());
  point_light_indices.reserve(frame.point_lights.size());
  box_indices.reserve(frame.boxes.size());
  assert(frame.cameras.size() <= std::numeric_limits<u32>::max());
  assert(frame.distant_lights.size() <= std::numeric_limits<u32>::max());
  assert(frame.point_lights.size() <= std::numeric_limits<u32>::max());
  assert(frame.boxes.size() <= std::numeric_limits<u32>::max());
  for (auto i = u32{}; i != frame.cameras.size(); ++i) {
    auto const p = camera_indices.try_emplace(frame.cameras[i].key, i);
    assert(p.second);
  }
  for (auto i = u32{}; i != frame.distant_lights.size(); ++i) {
    auto const b =
      distant_light_indices.try_emplace(frame.distant_lights[i].key, i);
    assert(b.second);
  }
  for (auto i = u32{}; i != frame.point_lights.size(); ++i) {
    auto const b =
      point_light_indices.try_emplace(frame.point_lights[i].key, i);
    assert(b.second);
  }
  for (auto i = u32{}; i != frame.boxes.size(); ++i) {
    auto const b = box_indices.try_emplace(frame.boxes[i].key, i);
    assert(b.second);
  }
}

} // namespace fpsparty::scene
