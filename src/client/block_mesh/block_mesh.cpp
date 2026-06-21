#include "block_mesh.hpp"

namespace fpsparty::client {

void Block_mesh::add_face(
  math::vec3 inciting_position,
  math::scaled_axis3 position_tangent,
  math::scaled_axis3 position_bitangent,
  math::vec2 inciting_texcoord,
  math::scaled_axis2 texcoord_tangent,
  math::scaled_axis2 texcoord_bitangent,
  std::uint32_t texture_index) {
  auto const normal =
    static_cast<math::signed_axis3>(position_tangent)
      .cross(static_cast<math::signed_axis3>(position_bitangent));
  auto const &[normal_axis, normal_sign] = normal;
  auto const axis_index = static_cast<int>(normal_axis);
  auto const sign_index = static_cast<int>(normal_sign);
  auto &quad = _aligned_submeshes[axis_index][sign_index].faces.emplace_back();
  quad.vertices[0] = Grid_vertex{
    .position = inciting_position,
    .texcoord = inciting_texcoord,
    .texture_index = texture_index,
  };
  quad.vertices[1] = Grid_vertex{
    .position = inciting_position + static_cast<math::vec3>(position_tangent),
    .texcoord = inciting_texcoord + static_cast<math::vec2>(texcoord_tangent),
    .texture_index = texture_index,
  };
  quad.vertices[2] = Grid_vertex{
    .position = inciting_position + static_cast<math::vec3>(position_tangent) +
                static_cast<math::vec3>(position_bitangent),
    .texcoord = inciting_texcoord + static_cast<math::vec2>(texcoord_tangent) +
                static_cast<math::vec2>(texcoord_bitangent),
    .texture_index = texture_index,
  };
  quad.vertices[3] = Grid_vertex{
    .position = inciting_position + static_cast<math::vec3>(position_bitangent),
    .texcoord = inciting_texcoord + static_cast<math::vec2>(texcoord_bitangent),
    .texture_index = texture_index,
  };
  quad.occluder = [&] -> std::optional<math::signed_axis3> {
    if (normal_axis == math::axis3::x) {
      if (normal_sign == math::sign::positive) {
        if (inciting_position.x() == 1.0f) {
          return normal;
        }
      } else {
        if (inciting_position.x() == 0.0f) {
          return normal;
        }
      }
      return std::nullopt;
    } else if (normal_axis == math::axis3::y) {
      if (normal_sign == math::sign::positive) {
        if (inciting_position.y() == 1.0f) {
          return normal;
        }
      } else {
        if (inciting_position.y() == 0.0f) {
          return normal;
        }
      }
      return std::nullopt;
    } else {
      if (normal_sign == math::sign::positive) {
        if (inciting_position.z() == 1.0f) {
          return normal;
        }
      } else {
        if (inciting_position.z() == 0.0f) {
          return normal;
        }
      }
      return std::nullopt;
    }
  }();
}

} // namespace fpsparty::client
