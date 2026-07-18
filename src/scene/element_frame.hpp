#ifndef FPSPARTY_SCENE_ELEMENT_FRAME_HPP
#define FPSPARTY_SCENE_ELEMENT_FRAME_HPP

#include <vector>

#include <int.hpp>
#include <int_map.hpp>

#include "elements.hpp"

namespace fpsparty::scene {

struct Element_frame {
  std::vector<elements::Camera> cameras;
  std::vector<elements::Distant_light> distant_lights;
  std::vector<elements::Point_light> point_lights;
  std::vector<elements::Box> boxes;
};

struct Element_frame_index {
  explicit Element_frame_index(Element_frame const &frame);

  Int_map_nz<u64, u32> camera_indices;
  Int_map_nz<u64, u32> distant_light_indices;
  Int_map_nz<u64, u32> point_light_indices;
  Int_map_nz<u64, u32> box_indices;
};

} // namespace fpsparty::scene

#endif
