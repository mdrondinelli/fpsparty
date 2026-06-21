#include "block_mesh.hpp"

namespace fpsparty::client {

namespace {

auto const block_center = math::vec3{0.5f, 0.5f, 0.5f};

math::vec3 rotate_point_90(math::vec3 p, math::vec3 a) {
  auto const v = (p - block_center).eval();
  return block_center + a.cross(v) + a.dot(v) * a;
}

math::signed_axis3 ivec3_to_signed_axis3(math::ivec3 n) {
  if (n.x() == 1) {
    return {math::axis3::x, math::sign::positive};
  } else if (n.x() == -1) {
    return {math::axis3::x, math::sign::negative};
  } else if (n.y() == 1) {
    return {math::axis3::y, math::sign::positive};
  } else if (n.y() == -1) {
    return {math::axis3::y, math::sign::negative};
  } else if (n.z() == 1) {
    return {math::axis3::z, math::sign::positive};
  } else /*if (n.z() == -1)*/ {
    return {math::axis3::z, math::sign::negative};
  }
}

math::signed_axis3
rotate_signed_axis_90(math::signed_axis3 u, math::signed_axis3 a) {
  auto const av = static_cast<math::ivec3>(a);
  auto const uv = static_cast<math::ivec3>(u);
  return ivec3_to_signed_axis3((av.cross(uv) + av.dot(uv) * av).eval());
}

} // namespace

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

void Block_mesh::rotate_90(math::signed_axis3 normal) {
  auto const a = static_cast<math::vec3>(normal);
  auto rotated = std::array<std::array<Aligned_submesh, 2>, 3>{};
  for (auto axis_index = 0; axis_index < 3; ++axis_index) {
    for (auto sign_index = 0; sign_index < 2; ++sign_index) {
      auto const old_normal = math::signed_axis3{
        static_cast<math::axis3>(axis_index),
        static_cast<math::sign>(sign_index)};
      auto const new_normal = rotate_signed_axis_90(old_normal, normal);
      auto &dst = rotated[static_cast<int>(new_normal.axis)]
                         [static_cast<int>(new_normal.sign)]
                           .faces;
      for (auto &face : _aligned_submeshes[axis_index][sign_index].faces) {
        for (auto &vertex : face.vertices) {
          vertex.position = rotate_point_90(vertex.position, a);
        }
        if (face.occluder) {
          face.occluder = rotate_signed_axis_90(*face.occluder, normal);
        }
        dst.push_back(std::move(face));
      }
    }
  }
  _aligned_submeshes = std::move(rotated);
}

void Block_mesh::rotate_180(math::signed_axis3 normal) {
  rotate_90(normal);
  rotate_90(normal);
}

} // namespace fpsparty::client
