#include "scene.hpp"

namespace fpsparty::client {
game::Grid const &Scene::get_grid() const noexcept { return _grid; }

game::Grid &Scene::get_grid() noexcept { return _grid; }

std::vector<Mesh_instance> const &
Scene::get_mesh_instances() const noexcept {
  return _mesh_instances;
}

std::vector<Mesh_instance> &Scene::get_mesh_instances() noexcept {
  return _mesh_instances;
}

bool Scene::get_grid_remesh_flag() const noexcept {
  return _grid_remesh_flag;
}

void Scene::set_grid_remesh_flag() noexcept { _grid_remesh_flag = true; }

void Scene::clear_grid_remesh_flag() noexcept { _grid_remesh_flag = false; }
} // namespace fpsparty::client
