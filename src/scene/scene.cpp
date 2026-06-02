#include "scene.hpp"

namespace fpsparty::scene {
game::Grid const &Scene::get_grid() const noexcept { return _grid; }

game::Grid &Scene::get_grid() noexcept { return _grid; }

std::vector<Mesh_instance> const &
Scene::get_mesh_instances() const noexcept {
  return _mesh_instances;
}

std::vector<Mesh_instance> &Scene::get_mesh_instances() noexcept {
  return _mesh_instances;
}
} // namespace fpsparty::scene
