#ifndef FPSPARTY_SCENE_KEYFRAME_HPP
#define FPSPARTY_SCENE_KEYFRAME_HPP

#include <game/grid.hpp>

#include "component_frame.hpp"

namespace fpsparty::scene {

struct Keyframe {
  std::uint64_t number;
  game::Grid grid;
  Component_frame components;
};

} // namespace fpsparty::scene

#endif
