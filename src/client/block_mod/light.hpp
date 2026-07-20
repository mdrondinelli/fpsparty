#ifndef FPSPARTY_CLIENT_BLOCK_MOD_LIGHT_HPP
#define FPSPARTY_CLIENT_BLOCK_MOD_LIGHT_HPP

#include "block_mod.hpp"

namespace fpsparty::client {

class Light_block_mod : public Block_mod {
public:
  void init(Block_mod_init_info const &info) override;
};

} // namespace fpsparty::client

#endif
