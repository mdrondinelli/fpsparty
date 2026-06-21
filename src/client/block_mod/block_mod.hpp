#ifndef FPSPARTY_CLIENT_BLOCK_MOD_HPP
#define FPSPARTY_CLIENT_BLOCK_MOD_HPP

#include "../texture_manager.hpp"
#include "../block_texture_registry.hpp"
#include "../block_model_registry.hpp"

namespace fpsparty::client {

struct Block_mod_init_info {
  Texture_manager *texture_manager;
  Block_texture_registry *texture_registry;
  Block_model_registry *model_registry;
};

class Block_mod {
public:
  virtual ~Block_mod() = default;

  virtual void init(Block_mod_init_info const &info) = 0;
};

}

#endif
