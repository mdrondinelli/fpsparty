#ifndef FPSPARTY_CLIENT_BLOCK_MESH_BLOCK_MESH_HPP
#define FPSPARTY_CLIENT_BLOCK_MESH_BLOCK_MESH_HPP

#include <array>

#include <game/grid.hpp>
#include <math/axis.hpp>

#include "../grid_vertex.hpp"

namespace fpsparty::client {

struct Proto_grid_vertex {
  math::vec3 position;
  math::vec2 texcoord;
};

class Block_mesh {
public:
  struct Face {
    std::array<Grid_vertex, 4> vertices;
    std::optional<math::signed_axis3> occluder;
  };

  struct Aligned_submesh {
    std::vector<Face> faces;
  };

  void add_face(
    math::vec3 inciting_position,
    math::scaled_axis3 position_tangent,
    math::scaled_axis3 position_bitangent,
    math::vec2 inciting_texcoord,
    math::scaled_axis2 texcoord_tangent,
    math::scaled_axis2 texcoord_bitangent,
    std::uint32_t texture_index);

  Aligned_submesh const &aligned_submesh(math::signed_axis3 normal) const noexcept {
    return _aligned_submeshes[static_cast<int>(normal.axis)]
                             [static_cast<int>(normal.sign)];
  }

  std::array<std::array<Aligned_submesh, 2>, 3> const &
  aligned_submeshes() const noexcept {
    return _aligned_submeshes;
  }

private:
  std::array<std::array<Aligned_submesh, 2>, 3> _aligned_submeshes;
};

} // namespace fpsparty::client

#endif
