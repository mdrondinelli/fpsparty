#ifndef FPSPARTY_CLIENT_BLOCK_MESH_REGISTRY_HPP
#define FPSPARTY_CLIENT_BLOCK_MESH_REGISTRY_HPP

#include <game/block.hpp>

#include "block_model.hpp"

namespace fpsparty::client {

class Block_model_registry {
public:
  Block_model_registry();

  ~Block_model_registry();

  void add(game::Block block, int data, Block_model &&model);

  Block_model const *get(game::Block block, int data) const;

private:
  struct Impl;

  std::unique_ptr<Impl> _impl{};
};

} // namespace fpsparty::client

#endif
