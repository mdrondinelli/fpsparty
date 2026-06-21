#include "conveyor.hpp"

#include <game/block.hpp>

#include "../block_mesh/full.hpp"

namespace fpsparty::client {

void Conveyor_block_mod::init(Block_mod_init_info const &info) {
  auto const belt_texture_index =
    info.texture_registry->add(info.texture_manager->get(Texture::conveyor_belt));
  auto const side_texture_index =
    info.texture_registry->add(info.texture_manager->get(Texture::conveyor_side));
  auto mesh = Block_mesh{};
  // +x face
  mesh.add_face(
    math::vec3{1.0f, 0.5f, 1.0f},
    -1.0f * math::axis3::z,
    0.5f * math::axis3::y,
    math::vec2{0.0f, 1.0f},
    +1.0f * math::axis2::x,
    -1.0f * math::axis2::y,
    side_texture_index);
  // -x face
  mesh.add_face(
    math::vec3{0.0f, 0.5f, 0.0f},
    1.0f * math::axis3::z,
    0.5f * math::axis3::y,
    math::vec2{0.0f, 1.0f},
    1.0f * math::axis2::x,
    -1.0f * math::axis2::y,
    side_texture_index);
  // +y face
  mesh.add_face(
    math::vec3{0.0f, 1.0f, 1.0f},
    1.0f * math::axis3::x,
    -1.0f * math::axis3::z,
    math::vec2{0.0f, 1.0f},
    1.0f * math::axis2::x,
    -1.0f * math::axis2::y,
    belt_texture_index);
  // -y face
  mesh.add_face(
    math::vec3{1.0f, 0.5f, 1.0f},
    -1.0f * math::axis3::x,
    -1.0f * math::axis3::z,
    math::vec2{0.0f, 0.5f},
    1.0f * math::axis2::x,
    1.0f * math::axis2::y,
    belt_texture_index);
  // +z face
  mesh.add_face(
    math::vec3{0.0f, 0.5f, 1.0f},
    1.0f * math::axis3::x,
    0.5f * math::axis3::y,
    math::vec2{0.0f, 0.5f},
    1.0f * math::axis2::x,
    -0.5f * math::axis2::y,
    belt_texture_index);
  // -z face
  mesh.add_face(
    math::vec3{1.0f, 0.5f, 0.0f},
    -1.0f * math::axis3::x,
    0.5f * math::axis3::y,
    math::vec2{0.0f, 0.5f},
    1.0f * math::axis2::x,
    0.5f * math::axis2::y,
    belt_texture_index);
  info.model_registry->add(
    game::Block::conveyor,
    0,
    Block_model{
      .mesh = std::move(mesh),
      .neighbor_occlusion_flags = 1 << (+math::axis3::y).index(),
    });
}

} // namespace fpsparty::client
