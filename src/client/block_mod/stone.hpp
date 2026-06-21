#ifndef FPSPARTY_CLIENT_BLOCK_MOD_STONE_HPP
#define FPSPARTY_CLIENT_BLOCK_MOD_STONE_HPP

#include "block_mod.hpp"

namespace fpsparty::client {
  
class Stone_block_mod : public Block_mod {
public:
  void init(Block_mod_init_info const &info) override;
};

}

#endif
