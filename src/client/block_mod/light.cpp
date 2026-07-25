#include "light.hpp"

#include <game/block.hpp>

#include "../block_mesh/full.hpp"

namespace fpsparty::client {

void Light_block_mod::init(Block_mod_init_info const &info) {
  auto texture_image = info.texture_manager->get(Texture::red);
  auto const texture_index =
    info.texture_registry->add(std::move(texture_image));
  info.model_registry->add(
    game::Block::light,
    0,
    Block_model{
      .mesh = make_full_block_mesh(texture_index),
      .neighbor_occlusion_flags = full_block_occlusion_flags,
      .rt_color = Rt_color::red,
      .emissivity_scale = 80.0f,
    });
}

} // namespace fpsparty::client
