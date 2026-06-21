#include "full.hpp"

namespace fpsparty::client {
  
Block_mesh make_full_block_mesh(u32 texture_index) {
  auto mesh = Block_mesh{};
  // +x face
  mesh.add_face(
      math::vec3{1.0f, 0.0f, 1.0f},
      -1.0f * math::axis3::z,
      1.0f * math::axis3::y,
      math::vec2{0.0f, 1.0f},
      +1.0f * math::axis2::x,
      -1.0f * math::axis2::y,
      texture_index);
  // -x face
  mesh.add_face(
      math::vec3{0.0f, 0.0f, 0.0f},
      1.0f * math::axis3::z,
      1.0f * math::axis3::y,
      math::vec2{0.0f, 1.0f},
      +1.0f * math::axis2::x,
      -1.0f * math::axis2::y,
      texture_index);
  // +y face
  mesh.add_face(
      math::vec3{0.0f, 1.0f, 1.0f},
      1.0f * math::axis3::x,
      -1.0f * math::axis3::z,
      math::vec2{0.0f, 1.0f},
      +1.0f * math::axis2::x,
      -1.0f * math::axis2::y,
      texture_index);
  // -y face
  mesh.add_face(
      math::vec3{1.0f, 0.0f, 1.0f},
      -1.0f * math::axis3::x,
      -1.0f * math::axis3::z,
      math::vec2{0.0f, 1.0f},
      +1.0f * math::axis2::x,
      -1.0f * math::axis2::y,
      texture_index);
  // +z face
  mesh.add_face(
      math::vec3{0.0f, 0.0f, 1.0f},
      1.0f * math::axis3::x,
      1.0f * math::axis3::y,
      math::vec2{0.0f, 1.0f},
      +1.0f * math::axis2::x,
      -1.0f * math::axis2::y,
      texture_index);
  // -z face
  mesh.add_face(
      math::vec3{1.0f, 0.0f, 0.0f},
      -1.0f * math::axis3::x,
      1.0f * math::axis3::y,
      math::vec2{0.0f, 1.0f},
      +1.0f * math::axis2::x,
      -1.0f * math::axis2::y,
      texture_index);
  return mesh;
}

}
