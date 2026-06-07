#ifndef FPSPARTY_CLIENT_SCENE_HPP
#define FPSPARTY_CLIENT_SCENE_HPP

#include "game/grid.hpp"
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <vector>

namespace fpsparty::client {
enum class Mesh {
  cube,
};

struct Mesh_instance {
  Mesh mesh{Mesh::cube};
  Eigen::Vector3f position{};
  Eigen::Quaternionf orientation{Eigen::Quaternionf::Identity()};
  Eigen::Vector3f scale{Eigen::Vector3f::Ones()};
};

class Scene {
public:
  game::Grid const &get_grid() const noexcept;

  game::Grid &get_grid() noexcept;

  std::vector<Mesh_instance> const &get_mesh_instances() const noexcept;

  std::vector<Mesh_instance> &get_mesh_instances() noexcept;

  bool get_grid_remesh_flag() const noexcept;

  void set_grid_remesh_flag() noexcept;

  void clear_grid_remesh_flag() noexcept;

private:
  game::Grid _grid{{}};
  std::vector<Mesh_instance> _mesh_instances{};
  bool _grid_remesh_flag{};
};
} // namespace fpsparty::client

#endif
