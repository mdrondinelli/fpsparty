#ifndef FPSPARTY_SCENE_COMPONENT_FRAME_HPP
#define FPSPARTY_SCENE_COMPONENT_FRAME_HPP

#include <vector>

#include <int.hpp>
#include <int_map.hpp>

#include "element_frame.hpp"
#include "components.hpp"

namespace fpsparty::scene {

struct Component_frame {
  Element_frame render() const;

  std::vector<elements::Camera> cameras;
  std::vector<elements::Distant_light> distant_lights;
  std::vector<elements::Point_light> point_lights;
  std::vector<elements::Box> boxes;
  std::vector<components::Humanoid> humanoids;
  std::vector<components::Item> items;
};

struct Component_frame_index {
  explicit Component_frame_index(Component_frame const &frame);

  Int_map_nz<u64, u32> camera_indices;
  Int_map_nz<u64, u32> distant_light_indices;
  Int_map_nz<u64, u32> point_light_indices;
  Int_map_nz<u64, u32> box_indices;
  Int_map_nz<net::Entity_id, u32> humanoid_indices;
  Int_map_nz<net::Entity_id, u32> item_indices;
};

Component_frame interpolate(
  Component_frame const &a,
  Component_frame const &b,
  Component_frame_index const &b_index,
  float t);

}

#endif
